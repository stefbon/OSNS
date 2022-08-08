/*

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-socket.h"
#include "libosns-connection.h"

#include "osns-protocol.h"

#include "receive.h"
#include "send.h"

static void start_read_receive_buffer(struct osns_receive_s *r);

static void dummy_cb(struct osns_packet_s *p, unsigned char type, char *data, unsigned int len, struct osns_control_s *ctrl)
{
    logoutput_debug("dummy_cb: received packet type %u len %u", type, len);
}

void init_osns_packet(struct osns_packet_s *packet)
{
    memset(packet, 0, sizeof(struct osns_packet_s));

    packet->status=0;
    packet->reply=0;
    packet->id=0;
    packet->ptr=NULL;
    packet->cb=dummy_cb;
    init_list_element(&packet->list, NULL);
    packet->size=0;
    packet->buffer=NULL;
}

/* read the bytes from the buffer, check all values are correct and process the packet  */

static void read_receive_buffer(void *ptr)
{
    struct osns_receive_s *r=(struct osns_receive_s *) ptr;
    struct connection_s *c=(struct connection_s *) r->ptr;
    struct shared_signal_s *signal=r->signal;
    unsigned int len=0;

    // logoutput_debug("read_receive_buffer");

    signal_lock(signal);

    if ((r->read==0) || (r->status & OSNS_RECEIVE_STATUS_WAIT) || (r->threads>1)) {

	signal_unlock(signal);
	return;

    }

    /* when not received enough data got into wait state 1 */

    r->status |= ((r->read < 4) ? OSNS_RECEIVE_STATUS_WAITING1 : 0);
    r->threads++;

    /* wait for:
    - enough data to read the header (==length) and
    - not other thread is processing the buffer to create a packet */

    while (r->status & (OSNS_RECEIVE_STATUS_WAITING1 | OSNS_RECEIVE_STATUS_PACKET)) {

	int result=signal_condwait(signal);

	if (r->read >= 4) {

	    r->status &= ~OSNS_RECEIVE_STATUS_WAITING1;

	} else if (r->read==0) {

	    r->status &= ~OSNS_RECEIVE_STATUS_WAITING1;
	    r->threads--;
	    signal_unlock(signal);
	    return;

	} else if (result>0) {

	    signal_unlock(signal);
	    (* c->ops.client.error)(c, NULL);

	} else if (r->status & OSNS_RECEIVE_STATUS_DISCONNECT) {

	    signal_unlock(signal);
	    return;

	}

    }

    r->status |= OSNS_RECEIVE_STATUS_PACKET; /* make sure there is only processing the buffer and reading and creating a packet */
    signal_unlock(signal);

    /* when here it's possible to read the first bytes */

    readpacket:

    len=get_uint32(r->buffer);

    if (len + 4 > r->size) {

	/* packet size overruns the buffer size: error */

	logoutput_warning("read_receive_buffer: tid %i packet length %i too big for buffer size %i", gettid(), len + 4, r->size);

	signal_lock(signal);
	r->status &= ~OSNS_RECEIVE_STATUS_PACKET;
	r->threads--;
	signal_broadcast(signal); 
	signal_unlock(signal);
	goto disconnect;

    } else {
	char data[len];
	struct osns_control_s ctrl;
	struct msghdr *msg=&r->msg;
	struct cmsghdr *cmsg=NULL;

	/* enough data ?*/

	// logoutput_debug("read_receive_buffer: len %i read %u", len, r->read);

	if (r->read < len + 4) {

	    signal_lock(signal);

	    while (r->read < len + 4) {

		r->status |= OSNS_RECEIVE_STATUS_WAITING2;
		int result=signal_condwait(signal);

		if (r->read >= len + 4) {

		    /* enough data read */
		    r->status &= ~OSNS_RECEIVE_STATUS_WAITING2;
		    break;

		} else if (result>0 || (r->status & OSNS_RECEIVE_STATUS_DISCONNECT)) {

		    r->status &= ~(OSNS_RECEIVE_STATUS_WAITING2 | OSNS_RECEIVE_STATUS_PACKET);
		    signal_unlock(signal);
		    goto disconnect;

		}

	    }

	    signal_unlock(signal);

	}

	/* copy packet data (minus the four bytes for the length) to a safe place */

	memcpy(data, (char *)(r->buffer + 4), len);

	/* in case of there is additional data (like fd) copy that */
	memset(&ctrl, 0, sizeof(struct osns_control_s));
	cmsg=CMSG_FIRSTHDR(msg);
	if (cmsg) {

	    if (cmsg->cmsg_level==SOL_SOCKET && cmsg->cmsg_type==SCM_RIGHTS) {

		ctrl.type=OSNS_CONTROL_TYPE_FD;
		memcpy(&ctrl.data.fd, CMSG_DATA(cmsg), sizeof(int));

	    }

	}

	/* lock the buffer since read position is moved and optionally data */

	signal_lock_flag(signal, &r->status, OSNS_RECEIVE_STATUS_BUFFER);
	r->read -= (len + 4);
	r->status &= ~OSNS_RECEIVE_STATUS_PACKET;
	r->threads--;

	if (r->read>0) {

	    /* there is more in buffer */

	    memmove(r->buffer, (char *)(r->buffer + len + 4), r->read);

	    if (r->threads==0) {

		/* start a new thread -> this thread is going futher processing the packet */

		start_read_receive_buffer(r);

	    } else {

		signal_broadcast(signal); /* signal the other thread data is ready to process */

	    }

	}

	signal_unlock_flag(signal, &r->status, OSNS_RECEIVE_STATUS_BUFFER);
	(* r->process_data)(r, data, len, &ctrl);

    }

    return;

    disconnect:
    (* c->ops.client.disconnect)(c, 0);

}

static void start_read_receive_buffer(struct osns_receive_s *r)
{
    // logoutput_debug("start_read_receive_buffer");
    work_workerthread(NULL, 0, read_receive_buffer, (void *) r, NULL);
}

void osns_read_available_data(struct osns_receive_s *r)
{
    struct connection_s *c=NULL;
    struct system_socket_s *sock=NULL;
    struct shared_signal_s *signal=NULL;
    int bytesread=0;
    unsigned int error=0;
    struct iovec iov[1];
    struct msghdr *msg=&r->msg;

    // logoutput_debug("osns_read_available_data");

    c=(struct connection_s *) r->ptr;
    sock=&c->sock;
    signal=r->signal;

    iov[0].iov_base=(void *) (r->buffer + r->read);
    iov[0].iov_len=(size_t)(r->size - r->read);

    msg->msg_name=NULL;
    msg->msg_namelen=0;
    msg->msg_iov=iov;
    msg->msg_iovlen=1;
    msg->msg_flags=0;

#ifdef __linux__

    /* assign buffer for control data/*/

    msg->msg_control=r->cmsg_buffer;
    msg->msg_controllen=OSNS_CMSG_BUFFERSIZE;

#else

    msg->msg_control=NULL;
    msg->msg_controllen=0;

#endif

    readbuffer:

    signal_lock_flag(signal, &r->status, OSNS_RECEIVE_STATUS_BUFFER);

    bytesread=socket_recvmsg(sock, msg);
    error=errno;

    if (bytesread<=0) {

	logoutput_debug("osns_read_available_data: error %i:%s", error, strerror(error));

	signal_unlock_flag(signal, &r->status, OSNS_RECEIVE_STATUS_BUFFER);

	/* handle error */

	if (socket_blocking_error(error)) {

	    /* for sure return here is the right action (does the eventloop create an event when not blocked anymore ??) */
	    return;

	} else if (bytesread==0) {

	    (* c->ops.client.disconnect)(c, 1);

	} else {

	    /* TODO: get error from socket: add error to socket operation */

	    (* c->ops.client.error)(c, NULL);

	}

    } else {

	// logoutput_debug("osns_read_available_data: %i", bytesread);

	/* no error */

	r->read+=bytesread;
	signal_unlock_flag(signal, &r->status, OSNS_RECEIVE_STATUS_BUFFER);

	/* start a thread (but max number of threads may not exceed 2)*/

	signal_lock(signal);

	if ((r->threads<2) && (r->status & OSNS_RECEIVE_STATUS_WAIT)==0) {

	    // logoutput_debug("osns_read_available_data: start a thread");
	    start_read_receive_buffer(r);

	} else {

	    /* there are threads waiting */
	    // logoutput_debug("osns_read_available_data: broadcast");
	    signal_broadcast(signal);

	}

	signal_unlock(signal);

    }

}

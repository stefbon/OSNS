/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Stef Bon <stefbon@gmail.com>

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
#include "libosns-misc.h"
#include "libosns-connection.h"
#include "libosns-datatypes.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"

#include "receive.h"

static void start_thread_read_receive_buffer(struct fuse_receive_s *r);

static void fuse_read_receive_buffer(void *ptr)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) ptr;
    struct shared_signal_s *signal=r->loop->signal;
    struct fuse_in_header *inh=(struct fuse_in_header *) r->buffer;

    logoutput_debug("fuse_read_receive_buffer");

    signal_lock(signal);

    if ((r->read==0) || (r->status & FUSE_RECEIVE_STATUS_WAIT) || (r->threads>1)) {

	signal_unlock(signal);
	return;

    }

    /* check there is enough data to read the header */

    r->status |= ((r->read < sizeof(struct fuse_in_header) ? FUSE_RECEIVE_STATUS_WAITING1 : 0));
    r->threads++;

    while (r->status & (FUSE_RECEIVE_STATUS_WAITING1 | FUSE_RECEIVE_STATUS_PACKET)) {

	int result=signal_condwait(signal);

	if (r->read >= sizeof(struct fuse_in_header)) {

	    r->status &= ~FUSE_RECEIVE_STATUS_WAITING1;
	    break;

	} else if (r->read==0) {

	    r->status &= ~FUSE_RECEIVE_STATUS_WAITING1;
	    r->threads--;
	    signal_unlock(signal);
	    return;

	} else if (result>0) {

	    r->status &= ~FUSE_RECEIVE_STATUS_WAITING1;
	    r->status |= FUSE_RECEIVE_STATUS_ERROR;
	    signal_unlock(signal);
	    goto disconnect;

	} else if (r->status & FUSE_RECEIVE_STATUS_DISCONNECT) {

	    r->status &= ~FUSE_RECEIVE_STATUS_WAITING1;
	    signal_unlock(signal);
	    goto disconnect;

	}

    }

    r->status |= FUSE_RECEIVE_STATUS_PACKET;
    signal_unlock(signal);

    /* from here this thread is the only one reading the buffer for a fuse message */

    {
	unsigned int size=inh->len;
	char data[size];
	struct fuse_in_header header;

	inh->len -= sizeof(struct fuse_in_header);

	if (size > r->size) {

	    logoutput_debug("fuse_read_receive_buffer: received a message too large for buffer (message %u buffer %u)", size, r->size);

	    signal_lock(signal);
	    r->status |= FUSE_RECEIVE_STATUS_DISCONNECT;
	    r->status &= ~FUSE_RECEIVE_STATUS_PACKET;
	    r->threads--;
	    signal_broadcast(signal);
	    signal_unlock(signal);
	    goto disconnect;

	}

	signal_lock(signal);

	memcpy(&header, inh, sizeof(struct fuse_in_header));

	if (r->read < size) {
	    unsigned int tmp=r->read;

	    /* not enough bytes received ... wait for more to arrive */

	    memcpy(data, (char *) (r->buffer + sizeof(struct fuse_in_header)), (tmp - sizeof(struct fuse_in_header)));
	    memmove(r->buffer, (char *)(r->buffer + tmp), size - tmp);
	    r->read=0;
	    r->status |= FUSE_RECEIVE_STATUS_WAITING2;

	    while (r->read < (size - tmp)) {

		int result=signal_condwait(signal);

		if (r->read >= (size - tmp)) {

		    r->status &= ~FUSE_RECEIVE_STATUS_WAITING2;
		    break;

		} else if ((result>0) || (r->status & (FUSE_RECEIVE_STATUS_DISCONNECT | FUSE_RECEIVE_STATUS_ERROR))) {

		    r->status &= ~(FUSE_RECEIVE_STATUS_WAITING2 | FUSE_RECEIVE_STATUS_PACKET);
		    signal_unlock(signal);
		    goto disconnect;

		}

	    }

	    memcpy(&data[tmp - sizeof(struct fuse_in_header)], r->buffer, (size - tmp));
	    r->read -= (size - tmp);

	} else {

	    memcpy(data, (char *) (r->buffer + sizeof(struct fuse_in_header)), inh->len);
	    r->read -= size;

	}

	r->status &= ~FUSE_RECEIVE_STATUS_PACKET;
	r->threads--;

	if (r->read>0) {

	    memmove(r->buffer, (char *)(r->buffer + size), r->read);

	    if (r->threads==0) {

		start_thread_read_receive_buffer(r);

	    } else {

		/* there are threads already reading the buffer */
		signal_broadcast(signal);

	    }

	}

	signal_unlock(signal);
	(* r->process_data)(r, &header, data);

    }

    return;

    disconnect:
    (* r->close_cb)(r, NULL);

}

static void start_thread_read_receive_buffer(struct fuse_receive_s *r)
{
    work_workerthread(NULL, 0, fuse_read_receive_buffer, (void *) r, NULL);
}

/* read data from socket and process it futher */

void handle_fuse_data_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) bevent->ptr;
    struct osns_socket_s *sock=bevent->sock;
    struct shared_signal_s *signal=r->loop->signal;
    int bytesread=0;
    unsigned int errcode=0;

    signal_lock(signal);

    bytesread=(* sock->sops.device.read)(sock, (char *)(r->buffer + r->read), (unsigned int)(r->size - r->read));
    errcode=errno;

    if (bytesread>0) {

	logoutput_debug("handle_fuse_data_event: %u bytes read buffer size %u read pos %u", (unsigned int) bytesread, r->size, r->read);

	r->read += bytesread;

	if ((r->threads < 2) && (r->status & FUSE_RECEIVE_STATUS_WAIT)==0) {

	    start_thread_read_receive_buffer(r);

	} else {

	    signal_broadcast(signal);

	}

	signal_unlock(signal);

    } else {

	signal_unlock(signal);

	if (bytesread==0) {

	    logoutput_debug("handle_fuse_data_event: connection closed");
	    goto disconnect;

	} else {

	    if (socket_connection_error(errcode)) {

		goto disconnect;

	    } else {

		logoutput_debug("handle_fuse_data_event: some error (%i:%s)", errcode, strerror(errcode));

	    }

	}

    }

    return;

    disconnect:
    (* r->close_cb)(r, bevent);

}

void handle_fuse_close_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) bevent->ptr;
    (* r->close_cb)(r, bevent);
}

void handle_fuse_error_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) bevent->ptr;
    (* r->error_cb)(r, bevent);
}

int fuse_socket_reply_error(struct osns_socket_s *sock, uint64_t unique, unsigned int errcode)
{
    struct iovec iov[1];
    struct fuse_out_header out;

    out.len=sizeof(struct fuse_out_header);
    out.error=-errcode;
    out.unique=unique;

    iov[0].iov_base=&out;
    iov[0].iov_len=out.len;

    return (* sock->sops.device.writev)(sock, iov, 1);
}

int fuse_socket_reply_data(struct osns_socket_s *sock, uint64_t unique, char *data, unsigned int size)
{
    struct iovec iov[2];
    struct fuse_out_header out;

    out.len=sizeof(struct fuse_out_header) + size;
    out.error=0;
    out.unique=unique;

    iov[0].iov_base=&out;
    iov[0].iov_len=sizeof(struct fuse_out_header);
    iov[1].iov_base=data;
    iov[1].iov_len=size;

    return (* sock->sops.device.writev)(sock, iov, 2);
}

int fuse_socket_notify(struct osns_socket_s *sock, unsigned int code, struct iovec *iov, unsigned int count)
{
    struct fuse_out_header out;

    out.len=sizeof(struct fuse_out_header);
    out.error=code;
    out.unique=0;

    iov[0].iov_base=&out;
    iov[0].iov_len=sizeof(struct fuse_out_header);

    return (* sock->sops.device.writev)(sock, iov, count);
}

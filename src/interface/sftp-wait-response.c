/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-resources.h"

#include "ssh/ssh-common.h"
#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/request-hash.h"
#include "sftp.h"

extern struct ssh_channel_s *get_ssh_channel_sftp_client(struct sftp_client_s *sftp);

/*	wait for a response on a request
	here are more signal which lead to a finish of the request:
	- response from the remote sftp server: SFTP_REQUEST_STATUS_RESPONSE and SFTP_REQUEST_STATUS_FINISH
	- no response from the remote sftp server: SFTP_REQUEST_STATUS_TIMEOUT
	- response from the process unit about the packet: packet invalid : SFTP_REQUEST_STATUS_ERROR
	- connection used for transport is closed (reason unknown)
	- original request from context is cancelled by the caller
*/

unsigned char wait_sftp_response_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r, struct system_timespec_s *timeout)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);
    struct shared_signal_s *signal=sftp->signal.signal;
    struct system_timespec_s expire=SYSTEM_TIME_INIT;
    int result=0;

    init_list_element(&sftp_r->list, NULL);
    get_current_time_system_time(&sftp_r->started);

    /* reuse the system time set earlier */

    copy_system_time(&expire, &sftp_r->started);
    system_time_add_time(&expire, timeout);
    copy_system_time(&sftp_r->timeout, timeout);

    /* add to the hash table */
    logoutput_debug("wait_sftp_response: add %u to hashtable", sftp_r->id);
    add_request_hashtable(sftp_r);

    signal_lock(signal);

    startcheckwait:

    if (sftp_r->status & SFTP_REQUEST_STATUS_FINISH) {

	/* not required to remove it from the hashtable, the thread handling
	    the response from the server has already done that */

	signal_unlock(signal);
	return 1;

    } else if (sftp_r->status & SFTP_REQUEST_STATUS_INTERRUPT) {

	/* 	test for additional events:
		- original sftp_r request is interrupted; this happens for example with a FUSE filesystem, the initiating fuse
		request can be interrupted; in this case the status of the sftp_r is also changed to INTERRUPT
		- waiting for a reply has been timedout, remote server has taken too long, or connection problems or channel closed etc etc
		- connection used for sftp is closed */

	if (sftp_r->status & SFTP_REQUEST_STATUS_RESPONSE) {

	    /* data is already received for this request, a thread is busy
		filling al the data, before setting the request to FINISH: let this continue */

	    sftp_r->status &= ~SFTP_REQUEST_STATUS_INTERRUPT;

	} else {

	    signal_unlock(signal);
	    /* interrupted: remove from hash */
	    remove_request_hashtable(sftp_r);
	    logoutput("wait_sftp_response: interrupted (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
	    return 0;

	}

    } else if (channel->flags & (CHANNEL_FLAG_SERVER_EOF | CHANNEL_FLAG_SERVER_CLOSE | CHANNEL_FLAG_EXIT_SIGNAL | CHANNEL_FLAG_EXIT_STATUS)) {

	signal_unlock(signal);
	remove_request_hashtable(sftp_r);
	sftp_r->status |= SFTP_REQUEST_STATUS_DISCONNECT;
	return 0;

    } else if (sftp_r->status & SFTP_REQUEST_STATUS_ERROR) {

	signal_unlock(signal);

	/* error : not required to remove from hash: thread setting this error has already done that */

	logoutput("wait_sftp_response: error (packet wrong format?) (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
	return 0;

    } else if (sftp->signal.seq==sftp_r->reply.sequence && system_time_test_earlier(&sftp_r->started, &sftp->signal.seqset)==1) {

	signal_unlock(signal);

	/* received a signal on the reply for this request: possibly something wrong with reply */

	logoutput("wait_sftp_response: received a signal on sequence (packet wrong format?) (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
	sftp_r->status=SFTP_REQUEST_STATUS_ERROR;
	remove_request_hashtable(sftp_r);
	return 0;

    } else if (sftp->signal.flags & SFTP_SIGNAL_FLAG_DISCONNECT) {

	signal_unlock(signal);

	/* signal from server/backend side:
	    channel closed and/or eof: remote (sub)system / process disconnected */

	logoutput("wait_sftp_response: connection for sftp disconnected");
	sftp_r->status |= SFTP_REQUEST_STATUS_DISCONNECT;
	remove_request_hashtable(sftp_r);
	return 0;

    }

    result=signal_condtimedwait(signal, &expire);

    if (result>0) {

	signal_unlock(signal);
	remove_request_hashtable(sftp_r);

	if (result==ETIMEDOUT) {

	    /* timeout: remove from hash */

	    logoutput("wait_sftp_response: timeout (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
	    sftp_r->status=SFTP_REQUEST_STATUS_TIMEDOUT;
	    return 0;

	} else {

	    logoutput("wait_sftp_response: error %i condition wait (%s)", result, strerror(result));
	    sftp_r->status=SFTP_REQUEST_STATUS_ERROR;
	    sftp_r->reply.error=result;
	    return 0;

	}

    } else if ((sftp_r->status & SFTP_REQUEST_STATUS_FINISH)==0) {

	goto startcheckwait;

    }

    unlock:
    signal_unlock(signal);

    out:
    return (sftp_r->status & SFTP_REQUEST_STATUS_FINISH) ? 1 : 0;

}

static int send_sftp_request_data_default(struct sftp_request_s *r, char *data, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    struct context_interface_s *i=r->interface;
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    int result=(* sftp->context.send_data)(sftp, data, size, seq, list);
    r->status |= SFTP_REQUEST_STATUS_SEND;
    return result;
}

static int send_sftp_request_data_blocked(struct sftp_request_s *r, char *data, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    logoutput_debug("send_sftp_request_data_blocked");
    r->reply.error=EINTR;
    return -1;
}

void set_sftp_request_blocked(struct sftp_request_s *r)
{
    r->send=send_sftp_request_data_blocked;
}

static void set_sftp_request_status(struct fuse_request_s *f_request)
{

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {
	struct sftp_request_s *r=(struct sftp_request_s *) f_request->ptr;

	if (r && (r->status & SFTP_REQUEST_STATUS_WAITING)) {

	    r->status|=SFTP_REQUEST_STATUS_INTERRUPT;
	    r->send=send_sftp_request_data_blocked;

	}

    }

}

void init_sftp_request(struct sftp_request_s *r, struct context_interface_s *i, struct fuse_request_s *f_request)
{

    memset(r, 0, sizeof(struct sftp_request_s));

    r->status = SFTP_REQUEST_STATUS_WAITING;
    r->interface=i;
    r->unique=i->unique;
    r->send=send_sftp_request_data_default;
    r->ptr=(void *) f_request;

    set_fuse_request_flags_cb(f_request, set_sftp_request_status);
    f_request->ptr=(void *) r;

}

void init_sftp_request_minimal(struct sftp_request_s *r, struct context_interface_s *i)
{

    memset(r, 0, sizeof(struct sftp_request_s));

    r->status = SFTP_REQUEST_STATUS_WAITING;
    r->interface=i;
    r->unique=i->unique;
    r->send=send_sftp_request_data_default;
    r->ptr=NULL;

}

void get_sftp_request_timeout_ctx(struct context_interface_s *i, struct system_timespec_s *timeout)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    get_sftp_request_timeout(sftp, timeout);
}

/*
  2010, 2011, 2012, 2103, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "logging.h"
#include "main.h"
#include "misc.h"

// #include "beventloop.h"
// #include "beventloop-timer.h"

#include "common-protocol.h"
#include "common.h"
#include "error.h"
#include "list.h"

/*

    maintain a hash table to attach response/data from server to the sftp request id
    as defined in draft-ietf-secsh-filexfer this value is a uint32_t and part of the sftp message

    here a "request" is created everytime a message is send to the server for sftp requests
    this request is stored in hashtbale using the request id
    when a reply is coming from the server, the corresponding request in the hashtable is
    looked up, and the data is stored there, and the waiting thread is signalled

    17 June 2016
    ------------
    At this moment there is one hashtable per session. This is ok for now, but actually wrong.
    There has to be one hashtable per channel, and in theory there can be more channels per session.
    This is a TODO.

    2 August 2016
    -------------
    A hashtable per channel.
    Make the size of the hashtable a default defined here.

    28 November 2016
    ----------------
    Add a new status: FINISH
    and make the signal_sftp_received set this status

    08 January 2017
    ---------------
    Add the interrupted unique

*/

static struct list_header_s hashtable[SFTP_SENDHASH_TABLE_SIZE_DEFAULT];
static pthread_mutex_t hashmutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hashcond=PTHREAD_COND_INITIALIZER;
static unsigned int hashsize=SFTP_SENDHASH_TABLE_SIZE_DEFAULT;
static unsigned char initdone=0;

static struct sftp_request_s *get_container_sftp_r(struct list_element_s *list)
{
    return (struct sftp_request_s *)(((char *) list) - offsetof(struct sftp_request_s, list));
}

static void add_request_hashtable(struct sftp_request_s *sftp_r)
{
    unsigned int hash=sftp_r->id % hashsize;
    struct list_header_s *header=&hashtable[hash];

    write_lock_list_header(header, &hashmutex, &hashcond);
    add_list_element_last(header, &sftp_r->list);
    write_unlock_list_header(header, &hashmutex, &hashcond);
}

static void remove_request_hashtable(struct sftp_request_s *sftp_r)
{
    unsigned int hash=sftp_r->id % hashsize;
    struct list_header_s *header=&hashtable[hash];

    write_lock_list_header(header, &hashmutex, &hashcond);
    remove_list_element(&sftp_r->list);
    write_unlock_list_header(header, &hashmutex, &hashcond);
}

/*	lookup the request in the request group hash
	the request id is used
	the request is removed from the hash table when found */

static struct sftp_request_s *lookup_request_hashtable(struct sftp_client_s *sftp, unsigned int id)
{
    unsigned int hash=id % hashsize;
    struct list_header_s *header=&hashtable[hash];
    struct sftp_request_s *sftp_r=NULL;
    struct list_element_s *list=NULL;

    /* writelock the hash table row
	it's very simple here since only write locks are */

    write_lock_list_header(header, &hashmutex, &hashcond);
    list=get_list_head(header, 0);

    while (list) {

	sftp_r=get_container_sftp_r(list);

	if (sftp_r->id==id && sftp_r->sftp==(void *) sftp) {

	    remove_list_element(list);
	    break;

	}

	list=get_next_element(list);
	sftp_r=NULL;

    }

    write_unlock_list_header(header, &hashmutex, &hashcond);
    return sftp_r;

}

/* lookup the original sftp request using the hash table by looking it up using the id */

struct sftp_request_s *get_sftp_request(struct sftp_client_s *sftp, unsigned int id, struct generic_error_s *error)
{
    struct sftp_request_s *sftp_r=NULL;

    sftp_r=lookup_request_hashtable(sftp, id);

    if (sftp_r) {

	if (sftp_r->status & SFTP_REQUEST_STATUS_WAITING) sftp_r->status -= SFTP_REQUEST_STATUS_WAITING;
	sftp_r->status|=SFTP_REQUEST_STATUS_RESPONSE;

    } else {

	/* request not found... this actually not an error but an event which can happen */

	if (error) set_generic_error_application(error, _ERROR_APPLICATION_TYPE_NOTFOUND, NULL, __PRETTY_FUNCTION__);

    }

    return sftp_r;

}

/*	signal the shared central mutex/cond
	called when a message is received to wake up any waiting request */

int signal_sftp_received_id(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    struct sftp_signal_s *signal=&sftp->signal;
    int result=0;

    pthread_mutex_lock(signal->mutex);

    if (sftp_r->status & SFTP_REQUEST_STATUS_RESPONSE) {

	sftp_r->status -= SFTP_REQUEST_STATUS_RESPONSE;
	sftp_r->status |= SFTP_REQUEST_STATUS_FINISH;

    } else {

	sftp_r->status |= SFTP_REQUEST_STATUS_ERROR;
	result=-1;

    }

    pthread_cond_broadcast(signal->cond);
    pthread_mutex_unlock(signal->mutex);
    return result;
}

void signal_sftp_received_id_error(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, struct generic_error_s *error)
{
    struct sftp_signal_s *signal=&sftp->signal;

    pthread_mutex_lock(signal->mutex);
    sftp_r->status |= SFTP_REQUEST_STATUS_ERROR;
    pthread_cond_broadcast(signal->cond);
    pthread_mutex_unlock(signal->mutex);
}

static void get_expire_time(struct timespec *expire, struct timespec *timeout)
{

    get_current_time(expire);

    expire->tv_sec+=timeout->tv_sec;
    expire->tv_nsec+=timeout->tv_nsec;

    if (expire->tv_nsec > 1000000000) {

	expire->tv_nsec -= 1000000000;
	expire->tv_sec++;

    }

}

/*	wait for a response on a request
	here are more signal which lead to a finish of the request:
	- response from the remote sftp server: SFTP_REQUEST_STATUS_RESPONSE and SFTP_REQUEST_STATUS_FINISH
	- no response from the remote sftp server: SFTP_REQUEST_STATUS_TIMEOUT
	- response from the process unit about the packet: packet invalid : SFTP_REQUEST_STATUS_ERROR
	- connection used for transport is closed (reason unknown)
	- original request from context is cancelled by the caller
*/

unsigned char wait_sftp_response(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, struct timespec *timeout)
{
    struct sftp_signal_s *signal=&sftp->signal;
    unsigned int hash=sftp_r->id % hashsize;
    struct list_header_s *header=&hashtable[hash];
    struct timespec expire;
    int result=0;

    get_expire_time(&expire, timeout);

    init_list_element(&sftp_r->list, header);
    get_current_time(&sftp_r->started);
    sftp_r->timeout.tv_sec=timeout->tv_sec;
    sftp_r->timeout.tv_nsec=timeout->tv_nsec;
    sftp_r->status=0;

    /* add to the hash table */
    add_request_hashtable(sftp_r);
    sftp_r->sftp=(void *) sftp;
    sftp_r->status|=SFTP_REQUEST_STATUS_WAITING;

    pthread_mutex_lock(signal->mutex);

    while ((sftp_r->status & (SFTP_REQUEST_STATUS_WAITING | SFTP_REQUEST_STATUS_RESPONSE)) && (signal->flags & SFTP_SIGNAL_FLAG_DISCONNECT)==0) {

	result=pthread_cond_timedwait(signal->cond, signal->mutex, &expire);

	if (sftp_r->status & SFTP_REQUEST_STATUS_FINISH) {

	    /* not required to remove it from the hashtable, the thread finishing this request has already done that */
	    break;

	} else {

	    /* 	test for additional events:
		- original sftp_r request is interrupted; this happens for example with a FUSE filesystem, the initiating fuse
		request can be interrupted; in this case the status of the sftp_r is also changed to INTERRUPT
		- waiting for a reply has been timedout, remote server has taken too long, or connection problems or channel closed etc etc
		- connection used for sftp is closed */

	    if (result==ETIMEDOUT) {

		pthread_mutex_unlock(signal->mutex);

		/* timeout: remove from hash */

		logoutput("wait_sftp_response: timeout (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
		sftp_r->status=SFTP_REQUEST_STATUS_TIMEDOUT;
		remove_request_hashtable(sftp_r);
		goto out;

	    } else if (sftp_r->status & SFTP_REQUEST_STATUS_INTERRUPT) {

		if (sftp_r->status & SFTP_REQUEST_STATUS_RESPONSE) {

		    /* data is already received for this request, a thread is busy
			filling al the data, before setting the request to FINISH: let this continue */

		    sftp_r->status -= SFTP_REQUEST_STATUS_INTERRUPT;
		    continue;

		}

		pthread_mutex_unlock(signal->mutex);

		/* interrupted: remove from hash */

		logoutput("wait_sftp_response: interrupted (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
		remove_request_hashtable(sftp_r);
		goto out;

	    } else if (sftp_r->status & SFTP_REQUEST_STATUS_ERROR) {

		pthread_mutex_unlock(signal->mutex);

		/* error : not required to remove from hash: thread setting this error has already done that */

		logoutput("wait_sftp_response: error (packet wrong format?) (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
		goto out;

	    } else if (signal->seq==sftp_r->reply.sequence &&
		(signal->seqset.tv_sec > sftp_r->started.tv_sec || (signal->seqset.tv_sec==sftp_r->started.tv_sec && signal->seqset.tv_nsec > sftp_r->started.tv_nsec))) {

		pthread_mutex_unlock(signal->mutex);

		/* received a signal on the reply for this request: possibly something wrong with reply */

		logoutput("wait_sftp_response: received a signal on sequence (packet wrong format?) (id=%i seq=%i)", sftp_r->id, sftp_r->reply.sequence);
		sftp_r->status=SFTP_REQUEST_STATUS_ERROR;
		remove_request_hashtable(sftp_r);
		goto out;

	    } else if (signal->flags & SFTP_SIGNAL_FLAG_DISCONNECT) {

		pthread_mutex_unlock(signal->mutex);

		/* signal from server/backend side:
		    channel closed and/or eof: remote (sub)system / process disconnected */

		logoutput("wait_sftp_response: connection for sftp disconnected");
		sftp_r->status |= SFTP_REQUEST_STATUS_DISCONNECT;
		remove_request_hashtable(sftp_r);
		goto out;

	    }

	}

    }

    pthread_mutex_unlock(signal->mutex);

    out:
    return (sftp_r->status & SFTP_REQUEST_STATUS_FINISH) ? 1 : 0;

}

unsigned char wait_sftp_service_complete(struct sftp_client_s *sftp, struct timespec *timeout)
{
    struct sftp_signal_s *signal=&sftp->signal;
    struct timespec expire;
    int result=-1;

    logoutput("wait_sftp_service_complete");

    get_expire_time(&expire, timeout);
    pthread_mutex_lock(signal->mutex);

    while ((signal->flags & (SFTP_SIGNAL_FLAG_DISCONNECT | SFTP_SIGNAL_FLAG_CONNECTED))==0) {

	result=pthread_cond_timedwait(signal->cond, signal->mutex, &expire);

	if (result>0) {

	    if (result!=ETIMEDOUT) logoutput("wait_sftp_service_complete: error pthread_cond_timedwait %i (%s)", result, strerror(result));

	    result=-1;
	    break;

	} else if (signal->flags & (SFTP_SIGNAL_FLAG_DISCONNECT | SFTP_SIGNAL_FLAG_CONNECTED)) {

	    result=0;
	    break;

	}

    }

    pthread_mutex_unlock(signal->mutex);
    return result;

}

void init_sftp_sendhash()
{
    pthread_mutex_lock(&hashmutex);
    if (initdone==0) {

	for (unsigned int i=0; i<(sizeof(hashtable)/sizeof(hashtable[0])); i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
	initdone=1;

    }
    pthread_mutex_unlock(&hashmutex);
}

void init_sftp_request(struct sftp_request_s *r)
{
    memset(r, 0, sizeof(struct sftp_request_s));
}

void clear_sftp_reply(struct sftp_reply_s *r)
{

    switch (r->type) {


	case SSH_FXP_VERSION:

	    if (r->response.init.buff) {

		free(r->response.init.buff);
		r->response.init.buff=NULL;

	    }

	    break;

	case SSH_FXP_HANDLE:

	    if (r->response.handle.name) {

		free(r->response.handle.name);
		r->response.handle.name=NULL;

	    }

	    break;

	case SSH_FXP_DATA:

	    if (r->response.data.data) {

		free(r->response.data.data);
		r->response.data.data=NULL;

	    }

	    break;

	case SSH_FXP_NAME:

	    if (r->response.names.buff) {

		free(r->response.names.buff);
		r->response.names.buff=NULL;

	    }

	    break;

	case SSH_FXP_ATTRS:

	    if (r->response.attr.buff) {

		free(r->response.attr.buff);
		r->response.attr.buff=NULL;

	    }

	    break;

    }

    memset(&r->response, '\0', sizeof(union sftp_response_u));

}

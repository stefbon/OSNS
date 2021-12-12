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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
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
#include <sys/syscall.h>

#define LOGGING
#include "log.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "smb.h"
#include "smb-signal.h"
#include "smb-wait-response.h"

#define SMB_SENDHASH_TABLE_SIZE_DEFAULT				64

static struct list_header_s hashtable[SMB_SENDHASH_TABLE_SIZE_DEFAULT];
static pthread_mutex_t hashmutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hashcond=PTHREAD_COND_INITIALIZER;
static unsigned int hashsize=SMB_SENDHASH_TABLE_SIZE_DEFAULT;
static unsigned char initdone=0;

static void add_request_hashtable(struct smb_request_s *smb_r)
{
    unsigned int hash=smb_r->id % hashsize;
    struct list_header_s *header=&hashtable[hash];

    write_lock_list_header(header, &hashmutex, &hashcond);
    add_list_element_last(header, &smb_r->list);
    write_unlock_list_header(header, &hashmutex, &hashcond);
}

static void remove_request_hashtable(struct smb_request_s *smb_r)
{
    unsigned int hash=smb_r->id % hashsize;
    struct list_header_s *header=&hashtable[hash];

    write_lock_list_header(header, &hashmutex, &hashcond);
    remove_list_element(&smb_r->list);
    write_unlock_list_header(header, &hashmutex, &hashcond);
}

uint32_t get_smb_unique_id(struct context_interface_s *interface)
{
    uint32_t id=0;

    pthread_mutex_lock(&hashmutex);
    id=get_id_smb_share(interface);
    pthread_mutex_unlock(&hashmutex);

    return id;
}

/*	lookup the request in the request group hash
	the request id is used
	the request is removed from the hash table when found */

static struct smb_request_s *lookup_request_hashtable(struct context_interface_s *interface, unsigned int id)
{
    unsigned int hash=id % hashsize;
    struct list_header_s *header=&hashtable[hash];
    struct smb_request_s *smb_r=NULL;
    struct list_element_s *list=NULL;
    struct context_interface_s *tmp=NULL;

    /* writelock the hash table row
	it's very simple here since only write locks are */

    write_lock_list_header(header, &hashmutex, &hashcond);
    list=get_list_head(header, 0);

    while (list) {

	smb_r=(struct smb_request_s *)((char *) list - offsetof(struct smb_request_s, list));
	tmp=smb_r->interface;

	if (smb_r->id==id && (tmp==interface)) {

	    remove_list_element(list);
	    break;

	}

	list=get_next_element(list);
	smb_r=NULL;

    }

    write_unlock_list_header(header, &hashmutex, &hashcond);
    return smb_r;

}

/* lookup the original sftp request using the hash table by looking it up using the id */

struct smb_request_s *get_smb_request(struct context_interface_s *interface, unsigned int id, struct generic_error_s *error)
{
    struct smb_request_s *smb_r=lookup_request_hashtable(interface, id);

    if (smb_r) {

	smb_r->status &= ~SMB_REQUEST_STATUS_WAITING;
	smb_r->status |= SMB_REQUEST_STATUS_RESPONSE;

    } else {

	/* request not found... this actually not an error but an event which can happen */

	if (error) set_generic_error_application(error, _ERROR_APPLICATION_TYPE_NOTFOUND, NULL, __PRETTY_FUNCTION__);

    }

    return smb_r;

}

/*	signal the shared central mutex/cond
	called when a message is received to wake up any waiting request */

int signal_smb_received_id(struct context_interface_s *interface, struct smb_request_s *r)
{
    struct smb_signal_s *signal=get_smb_signal_ctx(interface);
    int result=0;

    smb_signal_lock(signal);

    if (r->status & SMB_REQUEST_STATUS_RESPONSE) {

	r->status -= SMB_REQUEST_STATUS_RESPONSE;
	r->status |= SMB_REQUEST_STATUS_FINISH;

    } else {

	r->status |= SMB_REQUEST_STATUS_ERROR;
	result=-1;

    }

    smb_signal_broadcast(signal);
    smb_signal_unlock(signal);
    return result;
}

void signal_smb_received_id_error(struct context_interface_s *interface, struct smb_request_s *r, struct generic_error_s *error)
{
    struct smb_signal_s *signal=get_smb_signal_ctx(interface);

    smb_signal_lock(signal);
    r->status |= SMB_REQUEST_STATUS_ERROR;
    smb_signal_broadcast(signal);
    smb_signal_unlock(signal);
}

/* link the original fuse request and the following smb request */

static void set_smb_request_status(struct fuse_request_s *f_request)
{

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {
	struct smb_request_s *r=(struct smb_request_s *) f_request->followup;

	if (r) {

	    r->status |= SMB_REQUEST_STATUS_INTERRUPT;

	}

    }

}

void init_smb_request(struct smb_request_s *r, struct context_interface_s *i, struct fuse_request_s *f_request)
{

    memset(r, 0, sizeof(struct smb_request_s));

    r->status=SMB_REQUEST_STATUS_WAITING;
    r->interface=i;
    r->ptr=(void *) f_request;

    set_fuse_request_flags_cb(f_request, set_smb_request_status);
    f_request->followup=(void *) r;

}

void init_smb_request_minimal(struct smb_request_s *r, struct context_interface_s *i)
{

    memset(r, 0, sizeof(struct smb_request_s));

    r->status=SMB_REQUEST_STATUS_WAITING;
    r->interface=i;


}

unsigned char wait_smb_response_ctx(struct context_interface_s *interface, struct smb_request_s *r, struct system_timespec_s *timeout)
{
    struct smb_signal_s *signal=get_smb_signal_ctx(interface);
    unsigned int hash = (r->id % hashsize);
    struct list_header_s *header=&hashtable[hash];
    struct system_timespec_s expire=SYSTEM_TIME_INIT;
    int result=0;

    get_current_time_system_time(&r->started);

    /* reuse the system time set above */

    copy_system_time(&expire, &r->started);
    system_time_add_time(&expire, timeout);
    r->timeout.tv_sec=timeout->tv_sec;
    r->timeout.tv_nsec=timeout->tv_nsec;

    /* add to the hash table */
    add_request_hashtable(r);

    smb_signal_lock(signal);

    startcheckwait:

    if (r->status & SMB_REQUEST_STATUS_FINISH) {

	/* not required to remove it from the hashtable, the thread finishing this request has already done that */
	smb_signal_unlock(signal);
	return 1;

    } else if (r->status & SMB_REQUEST_STATUS_INTERRUPT) {

	    /* 	test for additional events:
		- the smb_r request is interrupted; this happens for example with a FUSE filesystem, the initiating fuse
		request can be interrupted; in this case the status of the smb_r is also changed to INTERRUPT
		- waiting for a reply has been timedout, remote server has taken too long, or connection problems etc etc
		- connection used for smb is closed */

	if (r->status & SMB_REQUEST_STATUS_RESPONSE) {

	    /* data is already received for this request, a thread is busy
		filling al the data, before setting the request to FINISH: let this continue */

	    r->status &= ~SMB_REQUEST_STATUS_INTERRUPT;

	} else {

	    /* interrupted: remove from hashtable */

	    smb_signal_unlock(signal);
	    logoutput("wait_smb_response: interrupted");
	    remove_request_hashtable(r);
	    r->status |= SMB_REQUEST_STATUS_WAITING;
	    r->error=EINTR;
	    return 0;

	}

    } else if (r->status & SMB_REQUEST_STATUS_ERROR) {

	smb_signal_unlock(signal);

	/* error : not required to remove from hash: thread setting this error has already done that */

	logoutput("wait_smb_response: error (packet wrong format?)");
	return 0;

    } else if (signal->flags & SMB_SIGNAL_FLAG_DISCONNECT) {

	smb_signal_unlock(signal);

	/* signal from server/backend side:
	    channel closed and/or eof: remote (sub)system / process disconnected */

	logoutput("wait_smb_response: connection for smb lost");
	remove_request_hashtable(r);
	r->status |= SMB_REQUEST_STATUS_DISCONNECT;
	r->error=ENOTCONN;
	return 0;

    }

    result=smb_signal_condtimedwait(signal, &expire);

    if (result>0) {

	smb_signal_unlock(signal);
	remove_request_hashtable(r);
	r->error=result;

	if (result==ETIMEDOUT) {

	    /* timeout */

	    logoutput("wait_smb_response: timeout");
	    r->status |= SMB_REQUEST_STATUS_TIMEDOUT;

	} else {

	    r->status |= SMB_REQUEST_STATUS_ERROR;

	}

	return 0;

    } else if ((r->status & SMB_REQUEST_STATUS_FINISH)==0) {

	goto startcheckwait;

    }

    result=1;
    smb_signal_unlock(signal);
    remove_request_hashtable(r);

    out:
    return result;

}

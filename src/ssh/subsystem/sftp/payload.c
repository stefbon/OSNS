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
#include <sys/statvfs.h>
#include <sys/mount.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "log.h"

#include "main.h"
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "mountinfo.h"

#include "misc.h"
#include "osns_sftp_subsystem.h"
#include "init.h"
#include "receive.h"
#include "send.h"
#include "payload.h"
#include "protocol.h"

/* process the payload queue */

static void process_sftp_payload_queue(void *ptr)
{
    struct sftp_subsystem_s *s=(struct sftp_subsystem_s *) ptr;
    struct sftp_payload_queue_s *queue=&s->queue;
    struct list_element_s *list=NULL;

    readqueue:

    pthread_mutex_lock(&queue->mutex);
    queue->threads++;
    list=get_list_head(&queue->header, SIMPLE_LIST_FLAG_REMOVE);
    pthread_mutex_unlock(&queue->mutex);

    if (list) {
	struct sftp_payload_s *payload=(struct sftp_payload_s *) ((char *) list - offsetof(struct sftp_payload_s, list));

	(* s->cb[payload->type])(payload);
	free(payload);
	if (s->flags & SFTP_SUBSYSTEM_FLAG_FINISH) goto finish;

	pthread_mutex_lock(&queue->mutex);
	queue->threads--;
	pthread_mutex_unlock(&queue->mutex);
	goto readqueue;

    }

    return;

    finish:

    pthread_mutex_lock(&queue->mutex);
    queue->threads--;
    if (queue->threads==0) {

	/* last thread: cleanup and close */
	finish_sftp_subsystem(s);

    }

    pthread_mutex_unlock(&queue->mutex);

}

/* the defauklt behaviour during normal session:
    queue the payload */

static void process_sftp_payload_session(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    struct sftp_connection_s *connection=&sftp->connection;
    struct sftp_payload_queue_s *queue=&sftp->queue;

    logoutput("process_sftp_payload_session: received %i bytes length %i type %i id %i", payload->len, payload->type, payload->id);

    pthread_mutex_lock(&queue->mutex);
    add_list_element_last(&queue->header, &payload->list);
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);

    work_workerthread(NULL, 0, process_sftp_payload_queue, (void *) connection, NULL);
    return;
}

static void process_sftp_payload_disconnect(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;

    logoutput("process_sftp_payload_disconnect: received type %i", payload->type);
    free(payload);
    finish_sftp_subsystem(sftp);
    return;
}

static void process_sftp_payload_notsupp(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;

    logoutput("process_sftp_payload_notsupp: received type %i", payload->type);

    if (reply_sftp_status_simple(sftp, payload->id, SSH_FX_OP_UNSUPPORTED)==-1) {

	logoutput_info("process_sftp_payload_notsupp: error sending");
    }

    return;
}

/* function to process the first (init) message */

static void process_sftp_payload_init(struct sftp_payload_s *payload)
{
    unsigned int clientversion=0;
    unsigned int pos=0;
    unsigned int length=0;
    char *data=payload->data;
    struct sftp_subsystem_s *sftp=payload->sftp;
    struct sftp_receive_s *receive=&sftp->receive;

    pthread_mutex_lock(&receive->mutex);

    if (sftp->flags & SFTP_SUBSYSTEM_FLAG_VERSION_RECEIVED) {

	/* error: version already received */

	logoutput_warning("process_sftp_init: init version message already received from client");
	pthread_mutex_unlock(&receive->mutex);
	goto disconnect;

    }

    logoutput_warning("process_sftp_init: received init version");
    sftp->flags |= SFTP_SUBSYSTEM_FLAG_VERSION_RECEIVED;

    /* set function to process to handle error:
	the client has to wait for reply! */

    set_process_sftp_payload_disconnect(sftp);
    pthread_mutex_unlock(&receive->mutex);

    if (payload->type != SSH_FXP_INIT) {

	logoutput_warning("process_sftp_init: type byte not SSH_FXP_INIT");
	goto disconnect;

    }

    clientversion=get_uint32(&data[pos]);
    pos+=4;

    if (clientversion<6) {

	logoutput("process_sftp_init: received unsupported version %i", clientversion);
	goto disconnect;

    }

    free(payload);
    payload=NULL;

    if (send_sftp_init(sftp)==0) {

	/* switch to processing "normal" payload */

	set_process_sftp_payload_session(sftp);
	sftp->flags |= SFTP_SUBSYSTEM_FLAG_VERSION_SEND;
	sftp->version=clientversion;

    } else {

	goto disconnect;

    }

    return;

    disconnect:

    if (payload) {

	free(payload);
	payload=NULL;

    }

    finish_sftp_subsystem(sftp);

}

static void set_process_sftp_payload(struct sftp_subsystem_s *sftp, void (* process_sftp_payload)(struct sftp_payload_s *p))
{
    sftp->receive.process_sftp_payload=process_sftp_payload;
}

void set_process_sftp_payload_init(struct sftp_subsystem_s *sftp)
{
    set_process_sftp_payload(sftp, process_sftp_payload_init);
}

void set_process_sftp_payload_session(struct sftp_subsystem_s *sftp)
{
    set_process_sftp_payload(sftp, process_sftp_payload_session);
}

void set_process_sftp_payload_disconnect(struct sftp_subsystem_s *sftp)
{
    set_process_sftp_payload(sftp, process_sftp_payload_disconnect);
}

void set_process_sftp_payload_notsupp(struct sftp_subsystem_s *sftp)
{
    set_process_sftp_payload(sftp, process_sftp_payload_notsupp);
}

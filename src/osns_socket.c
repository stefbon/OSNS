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

#include "log.h"

#include "main.h"
#include "misc.h"
#include "list.h"
#include "options.h"
#include "datatypes.h"
#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "lib/workspace/session.h"

#include "osns_socket.h"

#define OSNS_LOCALSOCKET_BUFFER_SIZE		9192

extern struct fs_options_s fs_options;

static void start_read_localsocket_buffer(struct osns_localsocket_s *localsocket);

static void disconnect_osns_localsocket(struct osns_localsocket_s *localsocket)
{
    struct socket_ops_s *sops=localsocket->connection.io.socket.sops;

    if (localsocket->connection.io.socket.bevent) {
	int fd = get_bevent_unix_fd(localsocket->connection.io.socket.bevent);

	if (fd>=0) {

	    close(fd);
	    set_bevent_unix_fd(localsocket->connection.io.socket.bevent, -1);

	}

	remove_bevent(localsocket->connection.io.socket.bevent);

    }

}

static void process_osns_packet(struct osns_localsocket_s *localsocket, struct osns_packet_s *packet)
{
    switch (packet->type) {

	case OSNS_MSG_VERSION:

	    break;

	case OSNS_MSG_DISCONNECT:

	    break;

	case OSNS_MSG_NOTSUPPORTED:

	    break;

	case OSNS_MSG_SERVICE_REQUEST:
	case OSNS_MSG_SERVICE_ACCEPT:
	case OSNS_MSG_SERVICE_DENY:

	    break;

	case OSNS_MSG_COMMAND:
	case OSNS_MSG_COMMAND_SUCCESS:
	case OSNS_MSG_COMMAND_FAILURE:

	    break;

	case OSNS_MSG_SSH_CHANNEL_OPEN:
	case OSNS_MSG_SSH_CHANNEL_OPEN_CONFIRMATION:
	case OSNS_MSG_SSH_CHANNEL_OPEN_FAILURE:
	case OSNS_MSG_SSH_CHANNEL_DATA:
	case OSNS_MSG_SSH_CHANNEL_EXTENDED_DATA:
	case OSNS_MSG_SSH_CHANNEL_EOF:
	case OSNS_MSG_SSH_CHANNEL_CLOSE:

	    break;

    }

}

static void read_localsocket_buffer(void *ptr)
{
    /* read the bytes from the buffer, check all values are correct and process the packet */
    struct osns_localsocket_s *localsocket=(struct osns_localsocket_s *) ptr;
    struct osns_packet_s packet;

    pthread_mutex_lock(&localsocket->mutex);

    if (localsocket->read==0 || (localsocket->status & OSNS_LOCALSOCKET_STATUS_WAIT) || localsocket->threads>1) {

	pthread_mutex_unlock(&localsocket->mutex);
	return;

    }

    localsocket->status |= ((localsocket->read < 4) ? OSNS_LOCALSOCKET_STATUS_WAITING1 : 0);
    localsocket->threads++;

    while (localsocket->status & (OSNS_LOCALSOCKET_STATUS_WAITING1 | OSNS_LOCALSOCKET_STATUS_PACKET)) {

	int result=pthread_cond_wait(&localsocket->cond, &localsocket->mutex);

	if (localsocket->read >= 4) {

	    localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_WAITING1;

	} else if (localsocket->read==0) {

	    localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_WAITING1;
	    localsocket->threads --;
	    pthread_mutex_unlock(&localsocket->mutex);
	    return;

	} else if (result>0 || (localsocket->status & OSNS_LOCALSOCKET_STATUS_DISCONNECT)) {

	    pthread_mutex_unlock(&localsocket->mutex);
	    return;

	}

    }

    localsocket->status |= OSNS_LOCALSOCKET_STATUS_PACKET;
    pthread_mutex_unlock(&localsocket->mutex);

    readpacket:

    packet.len=get_uint32(localsocket->buffer + localsocket->read);

    if (packet.len > localsocket->size) {

	logoutput_warning("read_localsocket_buffer: tid %i packet length %i too big for buffer size %i", gettid(), packet.len, localsocket->size);
	pthread_mutex_lock(&localsocket->mutex);
	localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_PACKET;
	localsocket->threads--;
	goto disconnect;

    } else {
	char data[packet.len];

	pthread_mutex_lock(&localsocket->mutex);

	while (localsocket->read < packet.len + 4) {

	    localsocket->status |= OSNS_LOCALSOCKET_STATUS_WAITING2;
	    int result=pthread_cond_wait(&localsocket->cond, &localsocket->mutex);

	    if (localsocket->read >= packet.len + 4) {

		localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_WAITING2;
		break;

	    } else if (result>0 || (localsocket->status & OSNS_LOCALSOCKET_STATUS_DISCONNECT)) {

		localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_WAITING2;
		localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_PACKET;
		pthread_mutex_unlock(&localsocket->mutex);
		goto disconnect;

	    }

	}

	memcpy(data, localsocket->buffer + 4, packet.len);
	localsocket->read -= (packet.len + 4);
	localsocket->status &= ~OSNS_LOCALSOCKET_STATUS_PACKET;
	localsocket->threads--;

	if (localsocket->read>0) {

	    memmove(localsocket->buffer, (char *)(localsocket->buffer + packet.len + 4), localsocket->read);
	    if (localsocket->threads==0) start_read_localsocket_buffer(localsocket);

	}

	pthread_cond_broadcast(&localsocket->cond);
	pthread_mutex_unlock(&localsocket->mutex);
	packet.buffer=data;

	/* packet is complete: ready to process it now */

	packet.type=data[0];
	process_osns_packet(localsocket, &packet);

    }

    return;

    disconnect:
    disconnect_osns_localsocket(localsocket);

}

static void start_read_localsocket_buffer(struct osns_localsocket_s *localsocket)
{
    work_workerthread(NULL, 0, read_localsocket_buffer, (void *) localsocket, NULL);
}

static int read_localsocket(struct osns_localsocket_s *localsocket, int fd)
{
    struct socket_ops_s *sops=localsocket->connection.io.socket.sops;
    int bytesread=0;
    unsigned int error=0;

    pthread_mutex_lock(&localsocket->mutex);

    readbuffer:

    bytesread=(* sops->recv)(&localsocket->connection.io.socket, (void *) (localsocket->buffer + localsocket->read), (size_t) (localsocket->size - localsocket->read), 0);
    error=errno;

    if (bytesread<=0) {

	pthread_mutex_unlock(&localsocket->mutex);

	logoutput_info("read_localsocket: bytesread %i error %i", bytesread, error);

	/* handle error */

	if (error==EAGAIN || error==EWOULDBLOCK) {

	    return 0;

	} else if (bytesread==0 || (error==ECONNRESET || error==ENOTCONN || error==EBADF || error==ENOTSOCK)) {

	    /* peer has performed an orderly shutdown */

	    disconnect_osns_localsocket(localsocket);

	} else {

	    logoutput_warning("read_localsocket: error %i:%s", error, strerror(error));

	}

    } else {

	/* no error */

	localsocket->read+=bytesread;

	if (localsocket->status & OSNS_LOCALSOCKET_STATUS_WAIT) {

	    /* there is a thread waiting for more data to arrive: signal it */

	    pthread_cond_broadcast(&localsocket->cond);

	} else if (localsocket->threads<2) {

	    /* start a thread (but max number of threads may not exceed 2)*/

	    start_read_localsocket_buffer(localsocket);

	}

	pthread_mutex_unlock(&localsocket->mutex);

    }

    return 0;

}

static void event_localsocket_cb(int fd, void *ptr, struct event_s *event)
{
    struct osns_localsocket_s *localsocket=(struct osns_localsocket_s *) ptr;

    if (signal_is_error(event) || signal_is_close(event)) {

	/* TODO */

    } else if (signal_is_data(event)) {

	int result=read_localsocket(localsocket, fd);

    }

}

static void init_connection_localsocket(struct fs_connection_s *c, unsigned int fd)
{
    struct fs_connection_s *s=c->ops.client.server;
    struct osns_localsocket_s *localsocket = (struct osns_localsocket_s *)((char *) c - offsetof(struct osns_localsocket_s, connection));
    struct bevent_s *bevent=create_fd_bevent(NULL, event_localsocket_cb, (void *) localsocket);

    if (bevent==NULL) return;

    set_bevent_unix_fd(bevent, (int) fd);
    set_bevent_watch(bevent, "i");

    if (add_bevent_beventloop(bevent)==0) {

	logoutput("init_connection_localsocket: added connection fd %i to eventloop", fd);
	s->io.socket.bevent=bevent;

    } else {

	logoutput("init_connection_localsocket: failed to add connection fd %i to eventloop", fd);
	if (bevent) remove_bevent(bevent);

    }

}


/* accept only connections from users with a complete session
    what api??
    SSH_MSG_CHANNEL_REQUEST...???
*/

struct fs_connection_s *accept_client_connection_from_localsocket(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *s_conn)
{
    struct osns_user_s *user=NULL;
    struct simple_lock_s wlock;
    struct osns_localsocket_s *localsocket=NULL;

    logoutput_info("accept_client_connection_from_localsocket");

    init_wlock_users_hash(&wlock);
    lock_users_hash(&wlock);
    user=lookup_osns_user(uid);

    if (user) {
	struct osns_localsocket_s *localsocket=malloc(sizeof(struct osns_localsocket_s) + OSNS_LOCALSOCKET_BUFFER_SIZE);

	if (localsocket) {

	    memset(localsocket, 0, sizeof(struct osns_localsocket_s) + OSNS_LOCALSOCKET_BUFFER_SIZE);

	    localsocket->status=0;
	    init_connection(&localsocket->connection, FS_CONNECTION_TYPE_LOCAL, FS_CONNECTION_ROLE_CLIENT);
	    pthread_mutex_init(&localsocket->mutex, NULL);
	    pthread_cond_init(&localsocket->cond, NULL);

	    localsocket->process_buffer=process_osns_packet;
	    localsocket->connection.ops.client.init=init_connection_localsocket;
	    localsocket->size=OSNS_LOCALSOCKET_BUFFER_SIZE;
	    localsocket->read=0;

	}

    }

    unlock:
    unlock_users_hash(&wlock);
    return ((localsocket) ? &localsocket->connection : NULL);
}

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

static void process_osns_packet(struct osns_localsocket_s *localsocket, struct osns_packet_s *packet)
{
}

static void read_localsocket_buffer(struct osns_localsocket_s *localsocket)
{
    /* read the bytes from the buffer, check all values are correct and process the packet */
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

	    remove_bevent_from_beventloop(&localsocket->connection.io.socket.bevent);
	    (* sops->close)(localsocket->connection.io.socket.bevent.fd);
	    localsocket->connection.io.socket.bevent.fd=-1;

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

	    read_localsocket_buffer(localsocket);

	}

	pthread_mutex_unlock(&localsocket->mutex);

    }

    return 0;

}

static int event_localsocket_cb(int fd, void *data, uint32_t eventcode)
{
    struct osns_localsocket_s *localsocket=(struct osns_localsocket_s *) data;
    int result=0;

    if (eventcode & (BEVENT_CODE_ERR | BEVENT_CODE_HUP)) {

	/* TODO */

    } else if (eventcode & BEVENT_CODE_IN) {

	result=read_localsocket(localsocket, fd);

    }

    return result;

}

static void init_connection_localsocket(struct fs_connection_s *c, unsigned int fd)
{
    struct fs_connection_s *s=c->ops.client.server;
    struct bevent_s *bevent=&s->io.socket.bevent;
    struct osns_localsocket_s *localsocket = (struct osns_localsocket_s *)((char *) c - offsetof(struct osns_localsocket_s, connection));

    if (add_to_beventloop(fd, BEVENT_CODE_IN, event_localsocket_cb, (void *) localsocket, &c->io.socket.bevent, bevent->loop)) {

	logoutput("init_connection_localsocket: added connection fd %i to eventloop", fd);

    } else {

	logoutput("init_connection_localsocket: failed to add connection fd %i to eventloop", fd);

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

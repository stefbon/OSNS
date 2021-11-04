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
#include "receive.h"
#include "connect.h"
#include "init.h"

static void read_sftp_buffer(void *ptr)
{
    struct sftp_subsystem_s *s=(struct sftp_subsystem_s *) ptr;
    struct sftp_connection_s *connection=&s->connection;
    struct sftp_receive_s *receive=&s->receive;
    unsigned int error=0;
    unsigned char headersize=9;
    uint32_t length=0;

    logoutput_debug("read_sftp_buffer: tid %i read %i", gettid(), receive->read);

    pthread_mutex_lock(&receive->mutex);

    start:

    if (receive->read==0 || (receive->flags & SFTP_RECEIVE_STATUS_WAIT) || receive->threads>1) {

	pthread_mutex_unlock(&receive->mutex);
	goto out;

    }

    /* for sftp 9 bytes is the bare minmum to read the header:
	- uint32		length
	- byte			type
	- uint32		id
    */

    receive->flags|=((receive->read < headersize) ? SFTP_RECEIVE_STATUS_WAITING1 : 0);
    receive->threads++;

    /* enough data in buffer and is the buffer free to process a packet ? */

    while (receive->flags & (SFTP_RECEIVE_STATUS_WAITING1 | SFTP_RECEIVE_STATUS_PACKET)) {

	int result=pthread_cond_wait(&receive->cond, &receive->mutex);

	if (receive->read >= headersize && (receive->flags & SFTP_RECEIVE_STATUS_WAITING1)) {

	    receive->flags &= ~SFTP_RECEIVE_STATUS_WAITING1;

	} else if (receive->read==0) {

	    receive->flags &= ~SFTP_RECEIVE_STATUS_WAITING1;
	    receive->threads--;
	    pthread_mutex_unlock(&receive->mutex);
	    goto out;

	} else if (result>0 || (receive->flags & SFTP_RECEIVE_STATUS_DISCONNECT)) {

	    pthread_mutex_unlock(&receive->mutex);
	    goto disconnect;

	}

    }

    /* there is no other thread creating and reading a packet from the buffer */

    receive->flags |= SFTP_RECEIVE_STATUS_PACKET;
    pthread_mutex_unlock(&receive->mutex);

    readpayload:

    length=get_uint32(receive->buffer);

    if (length + 4 > receive->size) {

	logoutput_warning("read_sftp_buffer: tid %i packet size %i too big (max %i)", gettid(), length + 4, receive->size);
	pthread_mutex_lock(&receive->mutex);
	receive->flags &= ~SFTP_RECEIVE_STATUS_PACKET;
	receive->threads--;
	goto disconnect;

    } else {
	struct sftp_payload_s *payload=NULL;

	pthread_mutex_lock(&receive->mutex);

	/* wait for data to arrive */

	logoutput_debug("read_sftp_buffer: tid %i read %i length %i", gettid(), receive->read, length);

	while (receive->read < length + 4) {

	    receive->flags |= SFTP_RECEIVE_STATUS_WAITING2;
	    int result=pthread_cond_wait(&receive->cond, &receive->mutex);

	    if (receive->read >= length + 4) {

		receive->flags &= ~SFTP_RECEIVE_STATUS_WAITING2;
		break;

	    } else if (result>0 || (receive->flags & SFTP_RECEIVE_STATUS_DISCONNECT)) {

		receive->flags &= ~ (SFTP_RECEIVE_STATUS_PACKET | SFTP_RECEIVE_STATUS_WAITING2);
		receive->threads--;
		pthread_mutex_unlock(&receive->mutex);
		goto disconnect;

	    }

	}

	/* enough data available */

	payload=malloc(sizeof(struct sftp_payload_s) + length - 1);

	if (payload) {
	    unsigned char pos=4;
	    char *buffer=receive->buffer;

	    memset(payload, 0, sizeof(struct sftp_payload_s) + length - 1);
	    payload->sftp=s;
	    payload->len=length - 1;
	    init_list_element(&payload->list, NULL);
	    payload->type=(unsigned char) buffer[pos];
	    pos++;

	    memcpy(payload->data, &buffer[pos], length - 1);
	    logoutput_debug("read_sftp_buffer: process payload len %i id %i", payload->len, payload->id);

	    (* receive->process_sftp_payload)(payload);

	} else {

	    logoutput_warning("read_sftp_buffer: tid %i unable to allocate payload %i bytes", gettid(), length);
	    pthread_mutex_unlock(&receive->mutex);
	    goto disconnect;

	}

	receive->read -= (length + 4);
	receive->flags &= ~SFTP_RECEIVE_STATUS_PACKET;
	receive->threads--;

	if (receive->read>0) {

	    logoutput_debug("read_sftp_buffer: %i bytes still in buffer", receive->read);
	    memmove(receive->buffer, (char *)(receive->buffer + length + 4), receive->read);
	    if (receive->threads==0) goto start; /* notice receive mutex is still locked here */

	}

	pthread_cond_broadcast(&receive->cond);
	pthread_mutex_unlock(&receive->mutex);

    }

    out:

    return;

    disconnect:

    logoutput_warning("read_ssh_buffer_packet: ignoring received data");
    disconnect_sftp_connection(connection, 1);

}

static void start_thread_read_sftp_connection_buffer(struct sftp_subsystem_s *s)
{
    work_workerthread(NULL, 0, read_sftp_buffer, (void *) s, NULL);
}

static int read_sftp_connection_socket(struct sftp_subsystem_s *s, int fd, struct event_s *events)
{
    struct sftp_connection_s *c=&s->connection;
    struct sftp_receive_s *receive=&s->receive;
    unsigned int error=0;
    int bytesread=0;

    pthread_mutex_lock(&receive->mutex);

    /* read the first data coming from the remote server */

    readbuffer:

    bytesread=(* c->read)(c, (void *) (receive->buffer + receive->read), (size_t) (receive->size - receive->read));
    error=errno;

    if (bytesread<=0) {

	pthread_mutex_unlock(&receive->mutex);

	logoutput_info("read_sftp_connection_socket: bytesread %i error %i", bytesread, error);

	/* handle error */

	if (bytesread==0) {

	    /* peer has performed an orderly shutdown */

	    c->flags |= (SFTP_CONNECTION_FLAG_TROUBLE | SFTP_CONNECTION_FLAG_RECV_EMPTY);

	    if (error>0) {

		c->error=error;
		c->flags |= SFTP_CONNECTION_FLAG_RECV_ERROR;

	    }

	    start_thread_sftp_connection_problem(c);
	    return -1;

	} else if (error==EAGAIN || error==EWOULDBLOCK) {

	    return 0;

	} else if (socket_network_connection_error(error)) {

	    logoutput_warning("read_sftp_connection_socket: socket is not connected? error %i:%s", error, strerror(error));
	    c->error=error;
	    c->flags |= (SFTP_CONNECTION_FLAG_TROUBLE | SFTP_CONNECTION_FLAG_RECV_ERROR);
	    start_thread_sftp_connection_problem(c);

	} else {

	    logoutput_warning("read_sftp_connection_socket: error %i:%s", error, strerror(error));

	}

    } else {

	/* no error */

	receive->read+=bytesread;

	logoutput_debug("read_sftp_connection_socket: read %i", receive->read);

	if (receive->flags & SFTP_RECEIVE_STATUS_WAIT) {

	    /* there is a thread waiting for more data to arrive: signal it */

	    pthread_cond_broadcast(&receive->cond);

	} else if (receive->threads<2) {

	    /* start a thread (but max number of threads may not exceed 2)*/

	    start_thread_read_sftp_connection_buffer(s);

	}

	pthread_mutex_unlock(&receive->mutex);

    }

    return 0;

}

void read_sftp_connection_signal(int fd, void *ptr, struct event_s *event)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ptr;
    struct sftp_connection_s *connection=&sftp->connection;
    struct sftp_receive_s *receive=&sftp->receive;
    int result=0;

    if (signal_is_error(event)) {

	s->connection.flags |= SFTP_CONNECTION_FLAG_TROUBLE;
	start_thread_sftp_connection_problem(connection);

    } else if (signal_is_close(event)) {

	goto close;

    } else if (signal_is_data(event)) {

	result=read_sftp_connection_socket(sftp, fd, event);

    } else {

	logoutput_warning("read_sftp_connection_signal: event not reckognized (fd=%i) value events %i", fd, printf_event_uint(event));

    }

    return;

    close:

    pthread_mutex_lock(&receive->mutex);

    if (receive->flags & SFTP_RECEIVE_STATUS_DISCONNECT) {

	pthread_mutex_unlock(&receive->mutex);
	return;

    }

    receive->flags |= SFTP_RECEIVE_STATUS_DISCONNECTING;
    pthread_mutex_unlock(&receive->mutex);

    remove_sftp_connection_eventloop(connection);
    disconnect_sftp_connection(connection, 0);

    pthread_mutex_lock(&receive->mutex);
    receive->flags |= SFTP_RECEIVE_STATUS_DISCONNECTED;
    pthread_mutex_unlock(&receive->mutex);

    logoutput("read_sftp_connection_signal: disconnected.. exit..");
    stop_beventloop(NULL);

    return;

}

static void process_sftp_payload_dummy(struct sftp_payload_s *p)
{
    logoutput("process_sftp_subsystem_dummy");
    free(p);
}

int init_sftp_receive(struct sftp_receive_s *receive)
{
    memset(receive, 0, sizeof(struct sftp_receive_s));

    receive->flags=SFTP_RECEIVE_STATUS_INIT;
    pthread_mutex_init(&receive->mutex, NULL);
    pthread_cond_init(&receive->cond, NULL);

    receive->process_sftp_payload=process_sftp_payload_dummy;

    receive->read=0;
    receive->size=SFTP_RECEIVE_BUFFER_SIZE_DEFAULT;
    receive->threads=0;

    receive->buffer=malloc(receive->size);

    if (receive->buffer) {

	memset(receive->buffer, 0, receive->size);

    } else {

	logoutput_warning("init_sftp_receive: unable to allocate %i bytes", receive->buffer);
	return -1;

    }

    return 0;
}

void free_sftp_receive(struct sftp_receive_s *receive)
{

    pthread_mutex_destroy(&receive->mutex);
    pthread_cond_destroy(&receive->cond);

    if (receive->buffer) {
	free(receive->buffer);
	receive->buffer=NULL;
    }

}

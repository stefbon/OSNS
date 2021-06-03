/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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
#include <sys/epoll.h>

#include "log.h"
#include "main.h"
#include "threads.h"

#include "misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-receive.h"
#include "ssh-utils.h"
#include "ssh-connections.h"

/*
    read the data coming from server after the connection is created
    and queue it
*/

static int read_ssh_connection_socket(struct ssh_connection_s *connection, int fd, struct event_s *event)
{
    struct socket_ops_s *sops=connection->connection.io.socket.sops;
    struct ssh_receive_s *receive=&connection->receive;
    unsigned int error=0;
    int bytesread=0;

    pthread_mutex_lock(&receive->mutex);

    /* read the first data coming from the remote server */

    readbuffer:

    bytesread=(* sops->recv)(&connection->connection.io.socket, (void *) (receive->buffer + receive->read), (size_t) (receive->size - receive->read), 0);
    error=errno;

    // logoutput("read_ssh_data: bytesread %i", bytesread);

    if (bytesread<=0) {

	pthread_mutex_unlock(&receive->mutex);

	logoutput_info("read_ssh_connection_socket: bytesread %i error %i", bytesread, error);

	/* handle error */

	if (bytesread==0) {

	    /* peer has performed an orderly shutdown */

	    connection->flags |= SSH_CONNECTION_FLAG_TROUBLE;
	    connection->setup.flags |= SSH_SETUP_FLAG_RECV_EMPTY;
	    if (error>0) {

		connection->setup.error=error;
		connection->setup.flags |= SSH_SETUP_FLAG_RECV_ERROR;

	    }
	    start_thread_ssh_connection_problem(connection);
	    return -1;

	} else if (error==EAGAIN || error==EWOULDBLOCK) {

	    return 0;

	} else if (socket_network_connection_error(error)) {

	    logoutput_warning("read_ssh_connection_socket: socket is not connected? error %i:%s", error, strerror(error));
	    connection->flags |= SSH_CONNECTION_FLAG_TROUBLE;
	    connection->setup.error=error;
	    connection->setup.flags |= SSH_SETUP_FLAG_RECV_ERROR;
	    start_thread_ssh_connection_problem(connection);

	} else {

	    logoutput_warning("read_ssh_connection_socket: error %i:%s", error, strerror(error));

	}

    } else {

	/* no error */

	receive->read+=bytesread;

	if (receive->status & SSH_RECEIVE_STATUS_WAIT) {

	    /* there is a thread waiting for more data to arrive: signal it */

	    pthread_cond_broadcast(&receive->cond);

	} else if (receive->threads<2) {

	    /* start a thread (but max number of threads may not exceed 2)*/

	    read_ssh_connection_buffer(connection);

	}

	pthread_mutex_unlock(&receive->mutex);

    }

    return 0;

    disconnect:

    disconnect_ssh_connection(connection);
    return 0;

}

void read_ssh_connection_signal(int fd, void *ptr, struct event_s *event)
{
    struct ssh_connection_s *connection=(struct ssh_connection_s *) ptr;

    if (signal_is_error(event) || signal_is_close(event)) {

	/* the remote side disconnected */

        logoutput("read_ssh_connection_signal: connection break (fd=%i)", fd);
        connection->flags |= SSH_CONNECTION_FLAG_TROUBLE;
	start_thread_ssh_connection_problem(connection);

    } else if (signal_is_data(event)) {

	logoutput("read_ssh_connection_signal: data is available (fd=%i)", fd);
	int result=read_ssh_connection_socket(connection, fd, event);

    } else {

	logoutput_warning("read_ssh_connection_signal: event not reckognized (fd=%i) value events %i", fd, printf_event_uint(event));

    }

}

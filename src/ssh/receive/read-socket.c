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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-threads.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-receive.h"
#include "ssh-utils.h"
#include "ssh-connections.h"

static void _disconnect_ssh_connection(struct ssh_connection_s *sshc, unsigned char remote)
{

    if (change_ssh_connection_setup(sshc, "setup", 0, SSH_SETUP_FLAG_DISCONNECTING, SSH_SETUP_OPTION_XOR, NULL, 0)==0) {
	struct connection_s *c=&sshc->connection;

	(* c->ops.client.disconnect)(c, remote);
	change_ssh_connection_setup(sshc, "setup", 0, SSH_SETUP_FLAG_DISCONNECTED, 0, NULL, 0);

    }

}

/*
    read the data coming from server after the connection is created
    and queue it
*/

void read_ssh_connection_socket(struct connection_s *c)
{
    struct ssh_connection_s *sshc=(struct ssh_connection_s *)((char *)c - offsetof(struct ssh_connection_s, connection));
    struct system_socket_s *sock=&c->sock;
    struct ssh_receive_s *receive=&sshc->receive;
    unsigned int error=0;
    int bytesread=0;

    pthread_mutex_lock(&receive->mutex);

    /* read the first data coming from the remote server */

    readbuffer:

    bytesread=socket_recv(sock, (void *) (receive->buffer + receive->read), (size_t) (receive->size - receive->read), 0);
    error=errno;

    if (bytesread<=0) {

	pthread_mutex_unlock(&receive->mutex);

	logoutput_info("read_ssh_connection_socket: bytesread %i error %i", bytesread, error);

	/* handle error */

	if (bytesread==0) {

	    /* peer has performed an orderly shutdown */

	    _disconnect_ssh_connection(sshc, 1);
	    return;

	} else if (error==EAGAIN || error==EWOULDBLOCK) {

	    return;

	} else if (socket_connection_error(error)) {

	    logoutput_warning("read_ssh_connection_socket: socket is not connected? error %i:%s", error, strerror(error));
	    sshc->flags |= SSH_CONNECTION_FLAG_TROUBLE;
	    sshc->setup.error=error;
	    sshc->setup.flags |= SSH_SETUP_FLAG_RECV_ERROR;
	    start_thread_ssh_connection_problem(sshc);

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

	    read_ssh_connection_buffer(sshc);

	}

	pthread_mutex_unlock(&receive->mutex);

    }

    return;

}

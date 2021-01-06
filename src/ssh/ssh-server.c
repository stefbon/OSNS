/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "main.h"
#include "logging.h"

#include "common-utils/utils.h"
#include "common-utils/workerthreads.h"

#include "ssh-utils.h"
#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-hash.h"
#include "ssh-connections.h"
#include "ssh-hostinfo.h"
#include "ssh-keyexchange.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-data.h"
#include "ssh-channel.h"
#include "ssh-signal.h"
#include "ssh-userauth.h"
#include "ssh-connections.h"
#include "extensions/extension.h"
#include "alloc/init.h"

#define UINT32_T_MAX		0xFFFFFFFF

static unsigned char init_done=0;
static pthread_mutex_t init_mutex=PTHREAD_MUTEX_INITIALIZER;

struct ssh_server_s *create_ssh_server()
{
    struct ssh_server_s *server=malloc(sizeof(struct ssh_server_s));

    if (server) {

	memset(server, 0, sizeof(struct ssh_server_s));
	server->flag=SSH_SERVER_FLAG_ALLOC;

    }

}

int init_server_socket(struct ssh_server_s *server, unsigned char type)
{

    switch (type) {

    case FS_CONNECTION_TYPE_TCP4:
    case FS_CONNECTION_TYPE_TCP6:
    case FS_CONNECTION_TYPE_UDP4:
    case FS_CONNECTION_TYPE_UDP6:

	init_connection(&server->listen, type, FS_CONNECTION_ROLE_SERVER);
	break;

    default:

	return -1;

    }

    init_list_header(&server->sessions, SIMPLE_LIST_EMPTY, NULL);
    server->flag|=SSH_SERVER_FLAG_INIT;
    return 0;

}

struct ssh_server_session_helper_s {
    struct ssh_session_s 	*session;
    unsigned int		fd;
};

static void complete_ssh_session_connection_setup(void *ptr)
{
    struct ssh_server_session_helper_s *helper=(struct ssh_server_session_helper_s *) ptr;

    if (setup_ssh_session(helper->session, (int) helper->fd)==0) {

	logoutput("complete_ssh_session_connection_setup: setup finished");

    } else {

	logoutput("complete_ssh_session_connection_setup: setup failed");

    }

    free(helper);

}

static void init_ssh_session_connection_cb(struct fs_connection_s *connection, unsigned int fd)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct ssh_server_session_helper_s *helper=malloc(sizeof(struct ssh_server_session_helper_s));

    /* start a thread to setup the ssh session using this connection
	if not using a thread the server connection will be occupied during this process
	which is a bad thing */

    if (helper) {

	memset(helper, 0, sizeof(struct ssh_server_session_helper_s));
	helper->session=get_ssh_connection_session(connection);
	helper->fd=fd;

	work_workerthread(NULL, 0, complete_ssh_session_connection_setup, (void *) helper, &error);

    } else {

	set_generic_error_system(&error, ENOMEM, __PRETTY_FUNCTION__);

    }

    if (error->value.errnum>0) logoutput("init_ssh_session_connection_cb: error %s starting thread to complete the session", get_description(error));
}

static struct fs_connection_s *accept_ssh_session_connection(struct host_address_s *host, struct fs_connection_s *server)
{
    struct generic_error_s error=GENERIC_HOST_INIT;
    struct ssh_session_s *session=NULL;
    struct fs_connection_s *connection=NULL;

    /* filter host 
	TODO: do not accept every host, depending the configuration */

    /* setup ssh session but then as server*/

    session=create_ssh_session(SSH_SESSION_FLAG_SERVER, &error);

    if (session==NULL) {

	logoutput("accept_ssh_session_connection: unable to create ssh session");
	goto error;

    }

    if (init_ssh_session(session, 0, NULL)==0) {

	logoutput("accept_ssh_session_connection: initialized ssh session");

    } else {

	logoutput("accept_ssh_session_connection: failed to initialize the ssh session");
	goto error;

    }

    /* add a connection to the session
	later it's possible through extensions/negotiatons another extra channel over UDP is added */

    if (add_main_ssh_connection(session)==0) {

	logoutput("accept_ssh_session_connection: added main connection to session");

    } else {

	logoutput("accept_ssh_session_connection: failed to add main connection to session");
	goto error;

    }

    connection=server->connections.main;
    connection->ops.client.init=init_ssh_server_connection_cb;
    return connection;

    error:

    _free_ssh_session(&session);
    return NULL;

}

int setup_ssh_server_socket(struct ssh_server_s *server, char *address, unsigned int port)
{
    int result=-1;
    struct generic_error_s error=GENERIC_ERROR_INIT;

    result=create_network_serversocket(address, port, &server->listen, NULL, accept_ssh_session_connection, &error);

    if (result==-1) {

	logoutput("setup_ssh_server_socket: unable to create network serversocket (%s:%i) error %s", address, port, get_description(&error));
	return -1;

    }

    

}

unsigned int get_ssh_server_buffer_size()
{
    return sizeof(struct ssh_server_s);
}

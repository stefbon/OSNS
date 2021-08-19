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
#include "log.h"
#include "options.h"

#include "misc.h"
#include "threads.h"

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

#include "ssh-userauth.h"
#include "ssh-connections.h"
#include "ssh-extensions.h"
#include "alloc/init.h"

#include "ssh-signal-client.h"

int init_ssh_identity_client(struct ssh_session_s *session, uid_t uid)
{
    struct ssh_identity_s *identity=&session->identity;
    struct passwd *result=NULL;

    init_ssh_identity(session);

    getpw:

    memset(&identity->pwd, 0, sizeof(struct passwd));
    result=NULL;
    identity->buffer=realloc(identity->buffer, identity->size);
    if(identity->buffer==NULL) goto error;

    if (getpwuid_r(uid, &identity->pwd, identity->buffer, identity->size, &result)==-1) {

	if (errno==ERANGE) {

	    identity->size+=128;
	    goto getpw; /* size buffer too small, increase and try again */

	}

	goto error;

    }

    logoutput("init_ssh_identity_client: found user %s (uid=%i, info %s) home %s", result->pw_name, result->pw_uid, result->pw_gecos, result->pw_dir);
    return 0;

    error:
    return -1;

}

unsigned int get_ssh_session_buffer_size()
{
    return sizeof(struct ssh_session_s);
}

int init_ssh_session_client(struct ssh_session_s *session, uid_t uid, void *ctx)
{
    logoutput("_init_ssh_session: init user uid %i", (unsigned int) uid);

    if (init_ssh_backend()==-1) goto error;

    init_ssh_session(session, uid, ctx);
    init_ssh_session_signals_client(&session->context);

    if (init_ssh_identity_client(session, uid)==-1) {

	logoutput("_init_ssh_session: error getting user identity for uid %i", (unsigned int) uid);
	goto error;

    }

    return 0;

    error:
    free_ssh_hostinfo(session);
    free_ssh_session_data(session);
    free_ssh_channels_table(session);
    free_ssh_identity(session);
    free_ssh_connections(session);
    return -1;

}

int connect_ssh_session_client(struct ssh_session_s *session, char *target, unsigned int port)
{
    struct ssh_connection_s *connection=NULL;
    struct ctx_option_s option;
    int fd=-1;
    struct common_signal_s *signal=NULL;

    /* get the ctx for values like:
	- shared mutex and cond for shared event signalling when for example the connection and/or
	is disconnected and the waiting thread wants to be informed about that (while waiting for a response)
	- timeout
    */

    memset(&option, 0, sizeof(struct ctx_option_s));
    option.type=_CTX_OPTION_TYPE_PVOID;
    if ((* session->context.signal_ssh2ctx)(session, "io:shared-signal", &option)>=0) {

	signal=(struct common_signal_s *) option.value.ptr;
	logoutput("connect_ssh_session_client: received shared workspace signal");

    }

    if (set_ssh_connections_signal(session, signal)==-1) {

	logoutput("connect_ssh_session_client: error setting shared signal");
	goto out;

    }

    memset(&option, 0, sizeof(struct ctx_option_s));
    option.type=_CTX_OPTION_TYPE_INT;
    if ((* session->context.signal_ssh2ctx)(session, "option:ssh.init_timeout", &option)>0) {

	session->config.connection_expire=option.value.integer;
	logoutput("connect_ssh_session_client: received connection timeout %i", option.value.integer);

    }

    if (add_main_ssh_connection(session)==0) {

	logoutput("connect_ssh_session_client: main connection added to session");

    } else {

	logoutput("connect_ssh_session_client: error adding main connection");
	goto out;

    }

    connection=session->connections.main;
    fd=connect_ssh_connection(connection, target, port);

    if (fd>0) {

	logoutput("connect_ssh_session_client: connected to %s:%i with fd %i", target, port, fd);

    } else {

	logoutput("connect_ssh_session_client: unable to connect to %s:%i", target, port);

    }

    out:

    return fd;

}

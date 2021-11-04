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

static int get_fd_incoming_data(struct sftp_connection_s *connection)
{
    int fd=-1;

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {
	struct fs_connection_s *fsc=&connection->type.std.stdin;

	/* STDIN is used for incoming data (nobrainer) */

	fd=get_bevent_unix_fd(fsc->io.std.bevent);

    }

    return fd;
}

static void cb_dummy(int fd, void *ptr, struct event_s *event)
{}

static int open_std_connection(struct sftp_connection_s *connection)
{
    struct sftp_subsystem_s *s=(struct sftp_subsystem_s *)((char *) connection - offsetof(struct sftp_subsystem_s, connection));
    struct fs_connection_s *fsc=NULL;
    struct io_std_s *io_std=NULL;
    struct std_ops_s *sops=NULL;

    /* STDIN */

    fsc=&connection->type.std.stdin;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    io_std->bevent=create_fd_bevent(NULL, read_sftp_connection_signal, (void *) s);
    if (io_std->bevent==NULL) goto error;
    set_bevent_watch(io_std->bevent, "i");
    set_bevent_unix_fd(io_std->bevent, (* sops->open)(io_std, 0));

    /* STDOUT */

    fsc=&connection->type.std.stdout;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    io_std->bevent=create_fd_bevent(NULL, cb_dummy, (void *) s);
    if (io_std->bevent==NULL) goto error;
    set_bevent_watch(io_std->bevent, "o");
    set_bevent_unix_fd(io_std->bevent, (* sops->open)(io_std, 0));

    /* STDERR */

    fsc=&connection->type.std.stderr;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    io_std->bevent=create_fd_bevent(NULL, cb_dummy, (void *) s);
    if (io_std->bevent==NULL) goto error;
    /* stderr is both directions */
    set_bevent_watch(io_std->bevent, "i");
    set_bevent_watch(io_std->bevent, "o");
    set_bevent_unix_fd(io_std->bevent, (* sops->open)(io_std, 0));

    return 0;

    error:

    /* close/remove any (b)event */

    logoutput("open_std_connection: error opening the fd's");

    fsc=&connection->type.std.stdin;
    io_std=&fsc->io.std;

    if (io_std->bevent) {

	remove_bevent(io_std->bevent);
	io_std->bevent=NULL;

    }

    fsc=&connection->type.std.stdout;
    io_std=&fsc->io.std;

    if (io_std->bevent) {

	remove_bevent(io_std->bevent);
	io_std->bevent=NULL;

    }

    fsc=&connection->type.std.stderr;
    io_std=&fsc->io.std;

    if (io_std->bevent) {

	remove_bevent(io_std->bevent);
	io_std->bevent=NULL;

    }

    return -1;

}

static int close_std_connection(struct sftp_connection_s *connection)
{
    struct fs_connection_s *fsc=&connection->type.std.stdin;
    struct io_std_s *io_std=&fsc->io.std;
    struct std_ops_s *sops=io_std->sops;
    int result=0;

    fsc=&connection->type.std.stdin;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    result=(* sops->close)(io_std);

    fsc=&connection->type.std.stdout;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    result=(* sops->close)(io_std);

    fsc=&connection->type.std.stderr;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    result=(* sops->close)(io_std);

    return 0;

}

static int read_std_connection(struct sftp_connection_s *connection, char *buffer, unsigned int size)
{
    struct fs_connection_s *fsc=&connection->type.std.stdin;
    struct io_std_s *io_std=&fsc->io.std;
    struct std_ops_s *sops=io_std->sops;

    /* STDIN is used for reading */

    return (* sops->read)(io_std, buffer, size);
}

static int write_std_connection(struct sftp_connection_s *connection, char *data, unsigned int size)
{
    struct fs_connection_s *fsc=&connection->type.std.stdout;
    struct io_std_s *io_std=&fsc->io.std;
    struct std_ops_s *sops=io_std->sops;

    /* STDOUT is used for writing */

    return (* sops->write)(io_std, data, size);
}

int init_sftp_connection(struct sftp_connection_s *connection, unsigned char type)
{
    /* default */

    if (type==0) type=SFTP_CONNECTION_TYPE_STD;

    /* depending type make connection */

    if (type==SFTP_CONNECTION_TYPE_STD) {

	connection->flags |= SFTP_CONNECTION_FLAG_STD;

	init_connection(&connection->type.std.stdin, FS_CONNECTION_TYPE_STD, FS_CONNECTION_ROLE_SERVER);
	set_io_std_type(&connection->type.std.stdin, "stdin");
	init_connection(&connection->type.std.stdout, FS_CONNECTION_TYPE_STD, FS_CONNECTION_ROLE_SERVER);
	set_io_std_type(&connection->type.std.stdout, "stdout");
	init_connection(&connection->type.std.stderr, FS_CONNECTION_TYPE_STD, FS_CONNECTION_ROLE_SERVER);
	set_io_std_type(&connection->type.std.stderr, "stderr");

	connection->open=open_std_connection;
	connection->close=close_std_connection;
	connection->read=read_std_connection;
	connection->write=write_std_connection;

    } else {

	logoutput_warning("connect_sftp_subsystem: type %i not reckognized: cannot continue", type);
	return -1;

    }

    return 0;

}

int connect_sftp_connection(struct sftp_connection_s *connection)
{

    if ((* connection->open)(connection)>=0) {

	logoutput_info("connect_sftp_connection: connected");
	return 0;

    }

    logoutput_warning("connect_sftp_connection: failed to connect");
    return -1;

}

int add_sftp_connection_eventloop(struct sftp_connection_s *connection)
{
    int result=-1;

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {
	struct fs_connection_s *fsc=&connection->type.std.stdin;
	struct bevent_s *bevent=fsc->io.std.bevent;

	if (add_bevent_beventloop(bevent)==0) {

	    logoutput_info("add_sftp_connection_eventloop: added %i to eventloop", get_bevent_unix_fd(bevent));
	    result=0;

	} else {

	    logoutput_warning("add_sftp_connection_eventloop: failed to add %i to eventloop", get_bevent_unix_fd(bevent));

	}

    }

    return result;

}

void remove_sftp_connection_eventloop(struct sftp_connection_s *connection)
{

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {
	struct fs_connection_s *fsc=&connection->type.std.stdin;
	struct bevent_s *bevent=fsc->io.socket.bevent;

	if (bevent) {

	    remove_bevent(bevent);
	    fsc->io.socket.bevent=NULL;

	}

    }

}

void disconnect_sftp_connection(struct sftp_connection_s *connection, unsigned char senddisconnect)
{

    if (connection->flags & (SFTP_CONNECTION_FLAG_DISCONNECTING | SFTP_CONNECTION_FLAG_DISCONNECTED)) return;

    connection->flags |= SFTP_CONNECTION_FLAG_DISCONNECTING;
    (* connection->close)(connection);
    connection->flags |= SFTP_CONNECTION_FLAG_DISCONNECTED;

}

void free_sftp_connection(struct sftp_connection_s *connection)
{

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {

	free_connection(&connection->type.std.stdin);
	free_connection(&connection->type.std.stdout);
	free_connection(&connection->type.std.stderr);

    }

}

unsigned int get_status_socket_connection(struct fs_connection_s *sc)
{
    int error=0;
    int fd=-1;

    if (sc->io.socket.bevent) fd=get_bevent_unix_fd(sc->io.socket.bevent);

    if (fd>=0) {
	socklen_t len=sizeof(int);
	struct socket_ops_s *sops=sc->io.socket.sops;

	if ((* sops->getsockopt)(fd, SOL_SOCKET, SO_ERROR, (void *) &error, &len)==0) {

	    logoutput("get_status_socket_connection: got error %i (%s)", error, strerror(error));

	} else {

	    error=errno;
	    logoutput("get_status_socket_connection: error %i (%s)", errno, strerror(errno));

	}

    } else {

	error=ENOTCONN;

    }

    return abs(error);

}

static void analyze_sftp_connection_problem(void *ptr)
{
    struct sftp_connection_s *connection=(struct sftp_connection_s *) ptr;
    unsigned int error=0;

    if (connection->flags & SFTP_CONNECTION_FLAG_DISCONNECT) return; /* already disconnect(ing/ed)*/

    if ((connection->flags & SFTP_CONNECTION_FLAG_TROUBLE)==0) {

	/* this flag should be set */
	logoutput("analyze_sftp_connection_problem: flag SFTP_CONNECTION_FLAG_TROUBLE not set as it should be ... warning");

    }

    if (connection->error>0) {

	error=connection->error;
	logoutput("analyze_sftp_connection_problem: found error %i:%s", error, strerror(error));

    } else {

	logoutput("analyze_sftp_connection_problem: errorcode unknown, trying to find it");

    }

    if (error==0) {
	struct fs_connection_s *sc=NULL;

        if (connection->flags & SFTP_CONNECTION_FLAG_STD) {

	    /* take the connection to read: stdin */

	    sc=&connection->type.std.stdin;

	}

	if (sc) {

	    error=get_status_socket_connection(sc);

	    if (error==0) {
		struct socket_ops_s *sops=sc->io.socket.sops;

		/* error==0 -> maybe a normal shutdown */

		size_t size=10; /* arbitrary size */
		char tmp[size];
		int bytesread=0;

		bytesread=(* sops->recv)(&sc->io.socket, (void *) tmp, size, MSG_PEEK);
		error=errno;

	    }

	}

    }

    if (error==0) {

	if (connection->flags & SFTP_CONNECTION_FLAG_RECV_EMPTY) error=ESHUTDOWN;

    }

    if (error>0) {

	switch (error) {

	    case EBADF:
	    case ENETDOWN:
	    case ENETUNREACH:
	    case ENETRESET:
	    case ECONNABORTED:
	    case ECONNRESET:
	    case ENOBUFS:
	    case ENOTCONN:
	    case ESHUTDOWN:
	    case ECONNREFUSED:
	    case EHOSTDOWN:
	    case EHOSTUNREACH:

		logoutput("analyze_sftp_connection_problem: error %i (%s): disconnecting", error, strerror(error));

		remove_sftp_connection_eventloop(connection);
		disconnect_sftp_connection(connection, 0);
		break;

	    default:

		logoutput("analyze_connection_problem: ignoring error %i (%s) : not reckognized", error, strerror(error));

	}

    }

}

int start_thread_sftp_connection_problem(struct sftp_connection_s *connection)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    work_workerthread(NULL, 0, analyze_sftp_connection_problem, (void *) connection, &error);
    return 0;
}

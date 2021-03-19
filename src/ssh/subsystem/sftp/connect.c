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

	fd=fsc->io.std.bevent.fd;

    }

    return fd;
}

static int open_std_connection(struct sftp_connection_s *connection)
{
    struct fs_connection_s *fsc=NULL;
    struct io_std_s *io_std=NULL;
    struct std_ops_s *sops=NULL;

    /* STDIN */

    fsc=&connection->type.std.stdin;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    io_std->bevent.fd=(* sops->open)(io_std, 0);

    /* STDOUT */

    fsc=&connection->type.std.stdout;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    io_std->bevent.fd=(* sops->open)(io_std, 0);

    /* STDERR */

    fsc=&connection->type.std.stderr;
    io_std=&fsc->io.std;
    sops=io_std->sops;

    io_std->bevent.fd=(* sops->open)(io_std, 0);

    return 0;
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
    struct fs_connection_s *fsc=&connection->type.std.stdin;
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
    int result=0;

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {
	struct fs_connection_s *fsc=&connection->type.std.stdin;
	struct bevent_s *bevent=&fsc->io.std.bevent;

	if (add_to_beventloop(bevent->fd, BEVENT_CODE_IN, read_sftp_connection_signal, (void *) connection, bevent, NULL)) {

	    logoutput_info("add_sftp_connection_eventloop: added %i to eventloop", bevent->fd);

	} else {

	    logoutput_warning("add_sftp_connection_eventloop: failed to add %i to eventloop", bevent->fd);
	    result=-1;

	}

    }

    return result;

}

void remove_sftp_connection_eventloop(struct sftp_connection_s *connection)
{

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {
	struct fs_connection_s *fsc=&connection->type.std.stdin;
	struct bevent_s *bevent=&fsc->io.socket.bevent;

	remove_bevent_from_beventloop(bevent);

    }

}

void disconnect_sftp_connection(struct sftp_connection_s *connection, unsigned char senddisconnect)
{
    (* connection->close)(connection);
}

void free_sftp_connection(struct sftp_connection_s *connection)
{

    if (connection->flags & SFTP_CONNECTION_FLAG_STD) {

	free_connection(&connection->type.std.stdin);
	free_connection(&connection->type.std.stdout);
	free_connection(&connection->type.std.stderr);

    }

}

int start_thread_sftp_connection_problem(struct sftp_connection_s *c)
{
}
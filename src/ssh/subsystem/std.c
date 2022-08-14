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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "connection.h"
#include "std.h"
#include "utils.h"

static void handle_std_connection_event_errorclose(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct ssh_subsystem_connection_s *connection=(struct ssh_subsystem_connection_s *) bevent->ptr;
    struct osns_socket_s *sock=get_bevent_osns_socket(bevent);

    if (flag & BEVENT_FLAG_CB_CLOSE) {

	logoutput_debug("handle_std_connection_event_errorclose: close");

	if ((&connection->type.std.stdin==sock) || (&connection->type.std.stdout==sock) || (&connection->type.std.stderr==sock)) {

	    (* connection->close)(connection, sock, 1);

	} else {

	    /* HUH ?? */

	}

    }

}

static void handle_stdin_connection_event_data(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct ssh_subsystem_connection_s *connection=(struct ssh_subsystem_connection_s *) bevent->ptr;
    struct osns_socket_s *sock=get_bevent_osns_socket(bevent);

    (* connection->read)(connection, sock);
}

static void handle_stderr_connection_event_data(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct ssh_subsystem_connection_s *connection=(struct ssh_subsystem_connection_s *) bevent->ptr;
    struct osns_socket_s *sock=get_bevent_osns_socket(bevent);

    (* connection->read_error)(connection, sock);
}

static int open_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection)
{
    struct osns_socket_s *sock=NULL;
    struct bevent_s *bevent=NULL;

    /* STDIN */

    sock=&connection->type.std.stdin;
    (* sock->set_unix_fd)(sock, fileno(stdin));
    bevent=create_fd_bevent(NULL, (void *) connection);

    if (bevent) {

	set_bevent_cb(bevent, (BEVENT_FLAG_CB_ERROR | BEVENT_FLAG_CB_CLOSE), handle_std_connection_event_errorclose);
	set_bevent_cb(bevent, (BEVENT_FLAG_CB_DATAAVAIL), handle_stdin_connection_event_data);
	set_bevent_osns_socket(bevent, sock);

	add_bevent_watch(bevent);

    } else {

	logoutput_debug("open_std_connection: unable to create bevent stdin");
	goto errorstdin;

    }

    /* STDOUT */

    sock=&connection->type.std.stdout;
    (* sock->set_unix_fd)(sock, fileno(stdout));
    bevent=create_fd_bevent(NULL, (void *) connection);

    if (bevent) {

	set_bevent_cb(bevent, (BEVENT_FLAG_CB_ERROR | BEVENT_FLAG_CB_CLOSE), handle_std_connection_event_errorclose);

	if (bevent_set_property(bevent, "edge-triggered", 1)==1) {

	    logoutput_debug("open_std_connection: edge triggering available: enable watched writes for stdout");
	    enable_bevent_write_watch(bevent);
	}

	set_bevent_osns_socket(bevent, sock);
	add_bevent_watch(bevent);

    } else {

	logoutput_debug("open_std_connection: unable to create bevent stdout");
	goto errorstdout;

    }

    /* STDERR */

    sock=&connection->type.std.stderr;
    (* sock->set_unix_fd)(sock, fileno(stderr));
    bevent=create_fd_bevent(NULL, (void *) connection);

    if (bevent) {

	set_bevent_cb(bevent, (BEVENT_FLAG_CB_ERROR | BEVENT_FLAG_CB_CLOSE), handle_std_connection_event_errorclose);
	set_bevent_cb(bevent, (BEVENT_FLAG_CB_DATAAVAIL), handle_stderr_connection_event_data);

	if (bevent_set_property(bevent, "edge-triggered", 1)==1) {

	    logoutput_debug("open_std_connection: edge triggering available: enable watched writes for stderr");
	    enable_bevent_write_watch(bevent); /* stderr is in- and output */

	}

	set_bevent_osns_socket(bevent, sock);
	add_bevent_watch(bevent);

    } else {

	logoutput_debug("open_std_connection: unable to create bevent stderr");
	goto errorstderr;

    }

    return 0;

    errorstderr:
    free_bevent_hlpr(&connection->type.std.stderr);

    errorstdout:
    free_bevent_hlpr(&connection->type.std.stdout);

    errorstdin:
    free_bevent_hlpr(&connection->type.std.stdin);

    return -1;

}


static int send_data_cb_default(struct osns_socket_s *sock, char *data, unsigned int size, void *ptr)
{
    return socket_write(sock, data, size);
}

int write_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection, char *data, unsigned int size)
{
    int byteswritten=-1;
    struct bevent_write_data_s wdata;
    struct osns_socket_s *sock=&connection->type.std.stdout; /* use stdout for writing */
    struct bevent_s *bevent=sock->event.link.bevent;

    wdata.flags=0;
    wdata.data=data;
    wdata.size=size;
    wdata.byteswritten=0;
    set_system_time(&wdata.timeout, 4, 0);
    wdata.ptr=NULL;
    init_generic_error(&wdata.error);

    return write_socket_signalled(bevent, &wdata, send_data_cb_default);
}

int init_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection)
{

    connection->flags |= SSH_SUBSYSTEM_CONNECTION_FLAG_STD;

    init_osns_socket(&connection->type.std.stdin, OSNS_SOCKET_TYPE_DEVICE, 0);
    init_osns_socket(&connection->type.std.stdout, OSNS_SOCKET_TYPE_DEVICE, OSNS_SOCKET_FLAG_WRONLY);
    init_osns_socket(&connection->type.std.stderr, OSNS_SOCKET_TYPE_DEVICE, OSNS_SOCKET_FLAG_RDWR);

    connection->open=open_ssh_subsystem_std;
    connection->close=close_ssh_subsystem_std;
    connection->write=write_ssh_subsystem_std;

    return 0;

}

void close_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned char free)
{

    /* STDIN */

    if ((sock==NULL) || (sock==&connection->type.std.stdin)) close_socket_hlpr(&connection->type.std.stdin, free);

    /* STDOUT */

    if ((sock==NULL) || (sock==&connection->type.std.stdout)) close_socket_hlpr(&connection->type.std.stdout, free);

    /* STDERR */

    if ((sock==NULL) || (sock==&connection->type.std.stderr)) close_socket_hlpr(&connection->type.std.stderr, free);

}

void clear_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection)
{
    close_ssh_subsystem_std(connection, NULL, 1);
}

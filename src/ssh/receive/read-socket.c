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
#include "libosns-eventloop.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-receive.h"
#include "ssh-utils.h"
#include "ssh-connections.h"

static uint64_t cb_copy_socket_ssh(struct osns_socket_s *sock, char *buffer, void *pmsg, unsigned int bytesread, void *ptr)
{
    sock->rd.pos += bytesread;
    signal_broadcast_locked(sock->signal);
    return 1;
}

/* socket msg cb's for ssh */

static unsigned int get_msg_header_size_ssh_greeter(struct osns_socket_s *sock, void *ptr)
{
    return 0; /* no header with greeter */
}

static unsigned int get_msg_header_size_ssh(struct osns_socket_s *sock, void *ptr)
{
    return 8; /* safe size */
}

static unsigned int get_msg_size_ssh(struct osns_socket_s *sock, char *buffer, unsigned int size, void *ptr)
{
    return (unsigned int) -1; /* set to maximum to get appending mode */
}

static void set_msg_size_ssh(struct osns_socket_s *sock, char *buffer, unsigned int size, void *ptr)
{}

struct close_ssh_connection_hlpr_s {
    struct ssh_connection_s *sshc;
};

static void process_ssh_socket_close(struct osns_socket_s *sock, unsigned int level, void *ptr)
{
    struct ssh_connection_s *sshc=(struct ssh_connection_s *) ptr;

    logoutput_debug("process_ssh_socket_close: level %u", level);
    process_socket_close_default(sock, level, NULL);

    if (change_ssh_connection_setup(sshc, "setup", 0, SSH_SETUP_FLAG_DISCONNECTING, SSH_SETUP_OPTION_XOR, NULL, 0)==0) {

        /* TODO: add a mechanism to inform the channels the connection is closed and signal every interface */

	change_ssh_connection_setup(sshc, "setup", 0, SSH_SETUP_FLAG_DISCONNECTED, 0, NULL, 0);

    }

}

static void process_ssh_socket_error(struct osns_socket_s *sock, unsigned int level, unsigned int errcode, void *ptr)
{
    struct ssh_connection_s *sshc=NULL;

    if (errcode==EAGAIN) return;

    sshc=(struct ssh_connection_s *) ptr;
    logoutput_debug("process_ssh_socket_error: level %u error %u (%s)", level, errcode, strerror(errcode));
    process_socket_error_default(sock, level, errcode, NULL);

}

static void process_ssh_socket_dummy(struct osns_socket_s *sock, char *header, char *data, struct socket_control_data_s *ctrl, void *ptr)
{}

void set_ssh_socket_behaviour(struct osns_socket_s *sock, const char *phase)
{
    struct bevent_s *bevent=sock->event.bevent;

    if (bevent==NULL) {

        logoutput_warning("set_ssh_socket_behaviour: cannot change socket eventloop behaviour ... no bevent set");

    } else {

        set_bevent_process_data_default(bevent);

    }

    if ((strcmp(phase, "init")==0) || (strcmp(phase, "greeter")==0)) {

        sock->ctx.read=cb_read_socket_ssh_greeter;
        sock->ctx.get_msg_header_size=get_msg_header_size_ssh_greeter;

    } else {

        sock->ctx.read=cb_read_socket_ssh;
        sock->ctx.get_msg_header_size=get_msg_header_size_ssh;

    }

}

static void cb_read_socket_ssh_dummy(struct osns_socket_s *sock, uint64_t ctr, void *ptr)
{}

void disable_ssh_socket_read_data(struct osns_socket_s *sock)
{
    sock->ctx.read=cb_read_socket_ssh_dummy;
}

void enable_ssh_socket_read_data(struct osns_socket_s *sock)
{
    sock->ctx.read=cb_read_socket_ssh;
}

void init_ssh_socket_behaviour(struct osns_socket_s *sock)
{

    sock->ctx.copy=cb_copy_socket_ssh;
    sock->ctx.get_msg_size=get_msg_size_ssh;
    sock->ctx.set_msg_size=set_msg_size_ssh;

    sock->ctx.process_data=process_ssh_socket_dummy;
    sock->ctx.process_close=process_ssh_socket_close;
    sock->ctx.process_error=process_ssh_socket_error;

    set_ssh_socket_behaviour(sock, "init");

}

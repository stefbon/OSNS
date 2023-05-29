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

#include "osns_sftp_subsystem.h"
#include "ssh/subsystem/connection.h"

#include "receive.h"
#include "init.h"

static unsigned int get_sftp_msg_header_size(struct osns_socket_s *s, void *ptr)
{
    return sizeof(struct sftp_in_header_s);
}

static unsigned int get_sftp_msg_size(struct osns_socket_s *s, char *buffer, unsigned int size, void *ptr)
{
    struct sftp_in_header_s *inh=(struct sftp_in_header_s *) buffer;

    /* return the length of the full message ...
    with sftp the length field in the header is
    the full length excluding the length field itself */

    return (inh->len + 4);

}

static void set_sftp_msg_size(struct osns_socket_s *s, char *buffer, unsigned int size, void *ptr)
{
    struct sftp_in_header_s *inh=(struct sftp_in_header_s *) buffer;
    inh->len=size;
}

static void process_sftp_data(struct osns_socket_s *sock, char *header, char *data, struct socket_control_data_s *ctrl, void *ptr)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ptr;
    struct sftp_in_header_s *inh=(struct sftp_in_header_s *) header;

    (* sftp->cb[inh->type])(sftp, inh, data);
}

static void process_sftp_close(struct osns_socket_s *sock, unsigned int level, void *ptr)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ptr;

    process_socket_close_default(sock, level, NULL);
    (* sftp->close)(sftp, level);
}

static void process_sftp_error(struct osns_socket_s *sock, unsigned int level, unsigned int errcode, void *ptr)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ptr;

    process_socket_error_default(sock, level, errcode, NULL);
    (* sftp->error)(sftp, level, errcode);
}

static void process_sftp_custom_data(struct osns_socket_s *sock, char *header, char *data, struct socket_control_data_s *ctrl, void *ptr)
{
    /* TODO */
    logoutput_debug("process_sftp_custom_data");
}

void init_sftp_socket_ops(struct osns_socket_s *sock, char *buffer, unsigned int size, unsigned char custom)
{

    sock->ctx.get_msg_header_size=get_sftp_msg_header_size;
    sock->ctx.get_msg_size=get_sftp_msg_size;
    sock->ctx.set_msg_size=set_sftp_msg_size;

    if (custom) {

        sock->ctx.process_data=process_sftp_custom_data;

    } else {

        sock->ctx.process_data=process_sftp_data;

    }

    sock->ctx.process_close=process_sftp_close;
    sock->ctx.process_error=process_sftp_error;
}

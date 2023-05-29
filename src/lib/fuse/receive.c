/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Stef Bon <stefbon@gmail.com>

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
#include "libosns-connection.h"
#include "libosns-datatypes.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"

#include "receive.h"

static unsigned int get_fuse_msg_header_size(struct osns_socket_s *sock, void *ptr)
{
    return sizeof(struct fuse_in_header);
}

static unsigned int get_fuse_msg_size(struct osns_socket_s *sock, char *header, unsigned int len, void *ptr)
{
    struct fuse_in_header *inh=(struct fuse_in_header *) header;
    return inh->len;
}

static void set_fuse_msg_size(struct osns_socket_s *sock, char *header, unsigned int len, void *ptr)
{
    struct fuse_in_header *inh=(struct fuse_in_header *) header;
    inh->len=len;
}

static void handle_fuse_data_event(struct osns_socket_s *sock, char *header, char *data, struct socket_control_data_s *ctrl, void *ptr)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) ptr;
    struct fuse_in_header *inh=(struct fuse_in_header *) header;
    (* r->process_data)(r, inh, data);
}

static void handle_fuse_close_event(struct osns_socket_s *sock, unsigned int level, void *ptr)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) ptr;
    process_socket_close_default(sock, level, NULL);
    (* r->close)(r, level);
}

static void handle_fuse_error_event(struct osns_socket_s *sock, unsigned int level, unsigned int errcode, void *ptr)
{
    struct fuse_receive_s *r=(struct fuse_receive_s *) ptr;
    process_socket_error_default(sock, level, errcode, NULL);
    (* r->error)(r, level, errcode);
}

void init_fuse_socket_ops(struct osns_socket_s *sock, char *buffer, unsigned int size)
{

    sock->ctx.get_msg_header_size=get_fuse_msg_header_size;
    sock->ctx.get_msg_size=get_fuse_msg_size;
    sock->ctx.set_msg_size=set_fuse_msg_size;

    sock->ctx.process_data=handle_fuse_data_event;
    sock->ctx.process_close=handle_fuse_close_event;
    sock->ctx.process_error=handle_fuse_error_event;

    sock->rd.buffer=buffer;
    sock->rd.size=size;
}

int fuse_socket_reply_error(struct osns_socket_s *sock, uint64_t unique, unsigned int errcode)
{
    struct iovec iov[1];
    struct fuse_out_header out;

    out.len=sizeof(struct fuse_out_header);
    out.error=-errcode;
    out.unique=unique;

    iov[0].iov_base=&out;
    iov[0].iov_len=out.len;

    return (* sock->sops.device.writev)(sock, iov, 1);
}

int fuse_socket_reply_data(struct osns_socket_s *sock, uint64_t unique, char *data, unsigned int size)
{
    struct iovec iov[2];
    struct fuse_out_header out;

    out.len=sizeof(struct fuse_out_header) + size;
    out.error=0;
    out.unique=unique;

    iov[0].iov_base=&out;
    iov[0].iov_len=sizeof(struct fuse_out_header);
    iov[1].iov_base=data;
    iov[1].iov_len=size;

    return (* sock->sops.device.writev)(sock, iov, 2);
}

int fuse_socket_notify(struct osns_socket_s *sock, unsigned int code, struct iovec *iov, unsigned int count)
{
    struct fuse_out_header out;

    out.len=sizeof(struct fuse_out_header);
    out.error=code;
    out.unique=0;

    iov[0].iov_base=&out;
    iov[0].iov_len=sizeof(struct fuse_out_header);

    return (* sock->sops.device.writev)(sock, iov, count);
}

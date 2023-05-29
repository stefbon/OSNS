/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_FUSE_RECEIVE_H
#define LIB_FUSE_RECEIVE_H

#include <linux/fuse.h>

#include "libosns-datatypes.h"
#include "libosns-socket.h"

#define OSNS_FUSE_VERSION				7
#define OSNS_FUSE_MINOR_VERSION				35

#define FUSE_RECEIVE_STATUS_BUFFER			1
#define FUSE_RECEIVE_STATUS_PACKET			2
#define FUSE_RECEIVE_STATUS_THREAD			4
#define FUSE_RECEIVE_STATUS_DISCONNECT			16
#define FUSE_RECEIVE_STATUS_ERROR			32

#define FUSE_RECEIVE_FLAG_INIT				1
#define FUSE_RECEIVE_FLAG_VERSION			2
#define FUSE_RECEIVE_FLAG_ERROR				4

struct fuse_receive_s {
    unsigned int				status;
    unsigned int				flags;
    void					*ptr;
    struct beventloop_s				*loop;
    struct shared_signal_s                      *signal;
    void					(* process_data)(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data);
    void                                        (* close)(struct fuse_receive_s *r, unsigned int level);
    void                                        (* error)(struct fuse_receive_s *r, unsigned int level, unsigned int errcode);
    void					(* set_interrupted)(struct fuse_receive_s *r, uint64_t unique);
    int						(* notify_VFS)(struct fuse_receive_s *r, unsigned int code, struct iovec *iov, unsigned int count);
    int						(* reply_VFS)(struct fuse_receive_s *r, uint64_t unique, char *data, unsigned int size);
    int						(* error_VFS)(struct fuse_receive_s *r, uint64_t unique, unsigned int code);
    struct osns_socket_s                        *sock;
};

/* prototypes */

void init_fuse_socket_ops(struct osns_socket_s *sock, char *buffer, unsigned int size);

int fuse_socket_reply_error(struct osns_socket_s *sock, uint64_t unique, unsigned int errcode);
int fuse_socket_reply_data(struct osns_socket_s *sock, uint64_t unique, char *data, unsigned int size);
int fuse_socket_notify(struct osns_socket_s *sock, unsigned int code, struct iovec *iov, unsigned int count);

#endif

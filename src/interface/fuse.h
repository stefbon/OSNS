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

#ifndef INTERFACE_FUSE_H
#define INTERFACE_FUSE_H

/* prototypes */

void init_fuse_interface();

void set_fuse_interface_eventloop(struct context_interface_s *i, struct beventloop_s *loop);
struct beventloop_s *get_fuse_interface_eventloop(struct context_interface_s *i);

void signal_fuse_request_interrupted(struct context_interface_s *interface, uint64_t unique);

struct fuse_config_s *get_fuse_interface_config(struct context_interface_s *i);
struct system_dev_s *get_fuse_interface_system_dev(struct context_interface_s *i);

int fuse_notify_VFS_delete(struct context_interface_s *interface, uint64_t pino, uint64_t ino, char *name, unsigned int len);
int fuse_reply_VFS_data(struct context_interface_s *i, uint64_t unique, char *data, unsigned int len);
int fuse_reply_VFS_error(struct context_interface_s *i, uint64_t unique, unsigned int errcode);

#endif

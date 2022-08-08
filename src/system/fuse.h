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

#ifndef OSNS_SYSTEM_FUSE_H
#define OSNS_SYSTEM_FUSE_H

/* prototypes */

void reply_VFS_error(struct fuse_receive_s *r, uint64_t u, unsigned int errcode);
void reply_VFS_data(struct fuse_receive_s *r, uint64_t u, char *data, unsigned int len);

void osns_system_process_fuse_data(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data);
void osns_system_process_fuse_close(struct fuse_receive_s *r, struct bevent_s *bevent);
void init_system_fuse();

#endif
/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _FUSE_SFTP_LOOKUP_H
#define _FUSE_SFTP_LOOKUP_H

void _sftp_lookup_entry_created(struct entry_s *entry, struct create_entry_s *ce);
void _sftp_lookup_entry_found(struct entry_s *entry, struct create_entry_s *ce);
void _sftp_lookup_entry_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int errcode);

void _fs_sftp_lookup(struct service_context_s *c, struct fuse_request_s *r, struct inode_s *inode, struct name_s *xname, struct fuse_path_s *fpath);

#endif

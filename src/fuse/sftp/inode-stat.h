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

#ifndef FUSE_SFTP_INODE_STAT_H
#define FUSE_SFTP_INODE_STAT_H

struct get_supported_sftp_attr_s {
    unsigned int				stat_mask_asked;
    unsigned int				stat_mask_result;
    unsigned int				len;
    struct sftp_valid_s				valid;
};

/* prototypes */

void set_local_attributes(struct context_interface_s *interface, struct inode_s *inode, struct system_stat_s *stat);
unsigned int get_attr_buffer_size(struct context_interface_s *interface, struct rw_attr_result_s *r, struct system_stat_s *stat, struct get_supported_sftp_attr_s *gssa);
void set_sftp_inode_stat_defaults(struct context_interface_s *interface, struct inode_s *inode);

int compare_cache_sftp(struct ssh_string_s *data, unsigned int size, char *buffer, void *ptr);
int test_remote_file_changed(struct system_stat_s *stat, struct system_timespec_s *mtime_before);

#endif

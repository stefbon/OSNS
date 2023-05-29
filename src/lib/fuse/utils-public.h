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

#ifndef _LIB_FUSE_UTILS_PUBLIC_H
#define _LIB_FUSE_UTILS_PUBLIC_H

#define FUSE_TIMEOUT_TYPE_ENTRY				1
#define FUSE_TIMEOUT_TYPE_ATTR				2
#define FUSE_TIMEOUT_TYPE_NEG				3

struct direntry_buffer_s {
    char							*data;
    unsigned int						pos;
    unsigned int						size;
    off_t							offset;
};

/* prototypes */

int add_direntry_buffer(struct fuse_config_s *c, struct direntry_buffer_s *buffer, struct name_s *xname, struct system_stat_s *stat);
int add_direntry_plus_buffer(struct fuse_config_s *c, struct direntry_buffer_s *buffer, struct name_s *xname, struct system_stat_s *stat);
void set_default_fuse_timeout(struct system_timespec_s *timeout, unsigned char w);
void set_rootstat(struct system_stat_s *stat);

#endif

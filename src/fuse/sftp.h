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

#ifndef FUSE_SFTP_H
#define FUSE_SFTP_H

struct sftp_service_s {
    char			*fullname;
    char			*name;
    char			*prefix;
    char			*uri;
};

/* prototypes */

struct service_context_s *add_shared_map_sftp(struct service_context_s *context, struct sftp_service_s *service, void *ptr);
unsigned int add_ssh_channel_sftp(struct service_context_s *context, char *fullname, unsigned int len, char *name, void *ptr);
unsigned int add_default_ssh_channel_sftp(struct service_context_s *context, void *ptr);

#endif

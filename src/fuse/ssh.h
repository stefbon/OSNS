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

#ifndef _FUSE_SSH_H
#define _FUSE_SSH_H

struct service_context_s *create_ssh_server_service_context(struct service_context_s *networkctx, struct interface_list_s *ilist, uint32_t unique);
unsigned int get_remote_services_ssh_server(struct service_context_s *context, unsigned int (* cb)(struct service_context_s *context, char *name, void *ptr), void *ptr);

#endif

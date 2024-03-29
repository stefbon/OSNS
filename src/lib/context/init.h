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

#ifndef _LIB_CONTEXT_INIT_H
#define _LIB_CONTEXT_INIT_H

#define CREATE_CONTEXT_FLAG_WORKSPACE_ALREADY_LOCKED			1

/* prototypes */

void set_name_service_context(struct service_context_s *ctx);
struct service_context_s *create_service_context(struct workspace_mount_s *workspace, struct service_context_s *parent, struct interface_list_s *ilist, unsigned char type, struct service_context_s *primary);
void free_service_context(struct service_context_s *context);

#endif

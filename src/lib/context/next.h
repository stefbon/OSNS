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

#ifndef _LIB_CONTEXT_NEXT_H
#define _LIB_CONTEXT_NEXT_H

/* prototypes */

struct workspace_mount_s *get_workspace_mount_ctx(struct service_context_s *ctx);

struct list_header_s *get_list_header_context(struct service_context_s *ctx);
struct list_element_s *get_list_element_context(struct service_context_s *ctx, const char *what);

unsigned int get_offset_list_header_context(unsigned int type);
unsigned int get_offset_list_element_context(unsigned int type, const char *what);
struct service_context_s *get_parent_context(struct service_context_s *ctx);

void add_service_context_workspace(struct workspace_mount_s *w, struct service_context_s *ctx);

void set_parent_service_context(struct service_context_s *pctx, struct service_context_s *ctx);
void unset_parent_service_context(struct service_context_s *pctx, struct service_context_s *ctx);

struct service_context_s *get_next_service_context(struct service_context_s *parent, struct service_context_s *context, const char *what);
struct service_context_s *get_next_shared_service_context(struct workspace_mount_s *w, struct service_context_s *ctx);

struct service_context_s *get_root_context_workspace(struct workspace_mount_s *w);
struct service_context_s *get_root_context(struct service_context_s *ctx);

int signal_selected_ctx(struct context_interface_s *i, unsigned char shared, const char *what, struct io_option_s *option, unsigned int type, int (* select)(struct context_interface_s *i, void *ptr), void *ptr);
struct service_context_s *find_service_context_unlocked(struct list_header_s *h, int (* compare)(struct service_context_s *ctx, void *ptr), void *ptr);

#endif

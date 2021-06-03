/*
  2010, 2011, 2012 Stef Bon <stefbon@gmail.com>

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

#ifndef _FUSE_NETWORK_H
#define _FUSE_NETWORK_H

struct discover_service_s {
    struct service_context_s						*networkctx;
    struct interface_list_s						*ailist;
    unsigned int							count;
};

#define FUSE_NETWORK_ACTION_FLAG_ADDED						1
#define FUSE_NETWORK_ACTION_FLAG_FOUND						2
#define FUSE_NETWORK_ACTION_FLAG_ERROR						3

struct service_context_s *create_network_browse_context(struct workspace_mount_s *w, struct service_context_s *parent, unsigned int ctxflags, unsigned int type, uint32_t unique);
struct entry_s *create_network_map_entry(struct service_context_s *context, struct directory_s *directory, struct name_s *xname, unsigned int *error);
struct entry_s *install_virtualnetwork_map(struct service_context_s *context, struct entry_s *parent, char *name, const char *what, unsigned char *p_action);

char *get_network_name(unsigned int flag);
unsigned int get_network_flag(struct name_s *xname);
unsigned int get_context_network_flag(struct service_context_s *context);

void start_discover_service_context_connect(struct workspace_mount_s *workspace);
void populate_network_workspace_mount(struct workspace_mount_s *workspace);

#endif

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

#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#define CHECK_INSTALL_CTX_ACTION_ADD				1
#define CHECK_INSTALL_CTX_ACTION_FOUND				2
#define CHECK_INSTALL_CTX_ACTION_RM				3

/* prototypes */

struct service_context_s *create_network_shared_context(struct workspace_mount_s *w, uint32_t unique, unsigned int service, unsigned int transport, unsigned int itype, struct service_context_s *primary, int (* compare)(struct service_context_s *ctx, void *ptr), void *ptr);
struct service_context_s *create_network_browse_context(struct workspace_mount_s *w, struct service_context_s *parent, unsigned int type, uint32_t unique, unsigned int service, struct service_context_s *primary);
struct service_context_s *check_create_install_context(struct workspace_mount_s *w, struct service_context_s *pctx, uint32_t unique, char *name, unsigned int service, struct service_context_s *primary, unsigned char *p_action);

void remove_context(struct workspace_mount_s *w, struct service_context_s *ctx);

#endif

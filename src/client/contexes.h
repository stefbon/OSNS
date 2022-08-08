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

#ifndef CLIENT_CONTEXES_H
#define CLIENT_CONTEXES_H

/* prototypes */

struct service_context_s *create_network_browse_context(struct workspace_mount_s *w, struct service_context_s *parent, unsigned int ctxflags, unsigned int type, uint32_t unique);
void populate_network_workspace_mount(struct service_context_s *pctx);

#endif

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

#ifndef CLIENT_WORKSPACES_H
#define CLIENT_WORKSPACES_H

struct client_session_s *get_client_session_workspace(struct workspace_mount_s *workspace);
struct service_context_s *create_mount_context(struct client_session_s *session, unsigned int type, unsigned int maxread);
void remove_workspaces(struct client_session_s *session);

int walk_interfaces_workspace(struct workspace_mount_s *w, int (* cb)(struct context_interface_s *i, void *ptr), void *ptr);

#endif

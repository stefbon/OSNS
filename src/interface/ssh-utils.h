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

#ifndef INTERFACE_SSH_UTILS_H
#define INTERFACE_SSH_UTILS_H

/* prototypes */

struct connection_s *get_connection_ssh_interface(struct context_interface_s *i);
unsigned int get_default_ssh_port(struct context_interface_s *i);

struct beventloop_s *get_ssh_session_eventloop(struct context_interface_s *i);
struct beventloop_s *get_workspace_beventloop(struct context_interface_s *i);
struct shared_signal_s *get_workspace_signal(struct context_interface_s *i);

#endif

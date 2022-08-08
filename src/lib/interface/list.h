/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_INTERFACE_LIST_H
#define _LIB_INTERFACE_LIST_H

#include "interface.h"

/* prototypes */

void init_header_interfaces();

unsigned char add_interface_ops(struct interface_ops_s *ops);
struct interface_ops_s *get_next_interface_ops(struct interface_ops_s *ops);
unsigned int build_interface_ops_list(struct context_interface_s *interface, struct interface_list_s *ilist, unsigned int start);
struct interface_list_s *get_interface_list(struct interface_list_s *ailist, unsigned int count, int type);

void clear_interface_buffer_default(struct context_interface_s *interface);

#endif

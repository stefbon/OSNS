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

#ifndef LIB_RESOURCES_LOCALHOST_H
#define LIB_RESOURCES_LOCALHOST_H

#include "libosns-network.h"
#include "libosns-datatypes.h"
#include "resource.h"

/* Prototypes */

#define LOOKUP_RESOURCE_FLAG_NONEXACT				1

struct resource_s *lookup_resource_id(uint32_t unique);

void add_resource_hash(struct resource_s *r);
uint32_t get_localhost_unique_ctr();
void remove_resource_hash(struct resource_s *r);

#define GET_RESOURCE_FLAG_UPDATE_USE				1

struct resource_s *get_next_hashed_resource(struct resource_s *resource, unsigned int flags);
void init_localhost_resources(struct shared_signal_s *signal);

void block_delete_resources();
void unblock_delete_resources();

void increase_refcount_resource(struct resource_s *r);
void decrease_refcount_resource(struct resource_s *r);

void set_changed(struct resource_s *r, struct system_timespec_s *c);

#endif

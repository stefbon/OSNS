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

#ifndef LIB_RESOURCES_RESOURCE_H
#define LIB_RESOURCES_RESOURCE_H

#include "libosns-network.h"
#include "libosns-datatypes.h"

#define RESOURCE_STATUS_BLOCK_DELETE		1
#define RESOURCE_STATUS_USE			2
#define RESOURCE_STATUS_CHANGE_TIME		4

#define RESOURCE_METHOD_DNSSD			(1 << 0)
#define RESOURCE_METHOD_STATICFILE		(1 << 1)

#define RESOURCE_FLAG_ALLOC			(1 << 0)
#define RESOURCE_FLAG_PRIVATE			(1 << 1)

#define RESOURCE_ACTION_ADD				1
#define RESOURCE_ACTION_RM				2
#define RESOURCE_ACTION_CHANGE				3
#define RESOURCE_ACTION_NEW				4

struct resource_s;

struct resource_subsys_s {
    char						*name;
    unsigned int					status;
    unsigned int					(* get_size)(struct resource_subsys_s *rss, const char *name);
    void						(* init)(struct resource_s *r);
    void						(* free)(struct resource_s *r);
    void						(* process_action)(struct resource_s *r, unsigned char what, void *ptr);
    void						*ptr;
};

struct resource_s {
    uint32_t						unique;
    struct resource_subsys_s				*subsys;
    const char						*name;
    unsigned int					status;
    unsigned int					flags;
    unsigned int					users;
    unsigned int					refcount;
    struct list_element_s				list;
    struct system_timespec_s				found;
    struct system_timespec_s				changed;
    unsigned int					size;
    char						buffer[];
};

/* Prototypes */
#endif

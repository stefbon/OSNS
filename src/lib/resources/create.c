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

#include "libosns-basic-system-headers.h"

#include <arpa/inet.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"

#include "resource.h"

struct resource_s *create_resource(struct resource_subsys_s *subsys, const char *name)
{
    unsigned int size=(* subsys->get_size)(subsys, name);
    struct resource_s *r=malloc(sizeof(struct resource_s) + size);

    logoutput_info("create_resource: subsys %s name %s size %u", subsys->name, name, size);

    if (r) {

	memset(r, 0, sizeof(struct resource_s) + size);

	r->unique=0;
	r->subsys=subsys;
	r->name=name;
	r->status=0;
	r->flags=RESOURCE_FLAG_ALLOC;
	r->users=0;
	r->refcount=0;

	init_list_element(&r->list, NULL);
	set_system_time(&r->found, 0, 0);
	set_system_time(&r->changed, 0, 0);
	get_current_time_system_time(&r->found);
	copy_system_time(&r->changed, &r->found);

	r->size=size;

	(* subsys->init)(r);

    }

    return r;

}

void free_resource(struct resource_s *r)
{
    free(r);
}


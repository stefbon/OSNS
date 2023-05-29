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

#include "libosns-log.h"
#include "libosns-workspace.h"
#include "list.h"

static struct list_header_s header=INIT_LIST_HEADER;

void init_header_interfaces()
{
    init_list_header(&header, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

unsigned char add_interface_ops(struct interface_ops_s *ops)
{
    unsigned char added=0;
    struct list_element_s *list=NULL;

    write_lock_list_header(&header);

    list=get_list_head(&header);
    while (list) {

	if (list==&ops->list) break;
	list=get_next_element(list);

    }

    if (list==NULL) {

	add_list_element_last(&header, &ops->list);
	added=1;
	logoutput_debug("add_interface_ops: name %s", ((ops->name) ? ops->name : "--unknown--"));

    } else {

	logoutput_debug("add_interface_ops: name %s already added", ((ops->name) ? ops->name : "--unknown--"));

    }

    write_unlock_list_header(&header);
    return added;
}

struct interface_ops_s *get_next_interface_ops(struct interface_ops_s *iops)
{
    struct list_element_s *next=((iops) ? get_next_element(&iops->list) : get_list_head(&header));
    return ((next) ? (struct interface_ops_s *)((char *) next - offsetof(struct interface_ops_s, list)) : NULL);
}

unsigned int build_interface_ops_list(struct context_interface_s *interface, struct interface_list_s *ailist, unsigned int start)
{
    struct interface_ops_s *iops=get_next_interface_ops(NULL);

    if (ailist) {

	for (unsigned int i=0; i<start; i++) {

	    ailist[i].type=-1;
	    ailist[i].name=NULL;
	    ailist[i].ops=NULL;

	}

    }

    while (iops) {

	start=(* iops->populate)(interface, iops, ailist, start);
	iops=get_next_interface_ops(iops);

    }

    return start;
}

struct interface_list_s *get_interface_list(struct interface_list_s *ailist, unsigned int count, int type)
{
    struct interface_list_s *ilist=NULL;

    logoutput_debug("get_interface_list: count %u type %i", count, type);

    for (unsigned int i=0; i<count; i++) {

	if (ailist[i].type==type) {
	    struct interface_ops_s *ops=ailist[i].ops;

	    ilist=&ailist[i];
	    logoutput_debug("get_interface_list: selected %u:%u %s - %s", i, type, ((ops->name) ? ops->name : "--unknown--"), ailist[i].name);
	    break;

	}

    }

    return ilist;
}

void clear_interface_buffer_default(struct context_interface_s *i)
{

    if (i->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	if ((i->flags & _INTERFACE_FLAG_BUFFER_CLEAR)==0) {

	    (* i->iocmd.in)(i, "command:close:", NULL, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
	    (* i->iocmd.in)(i, "command:free:", NULL, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
	    i->flags |= _INTERFACE_FLAG_BUFFER_CLEAR;

	}

	i->flags &= ~_INTERFACE_FLAG_BUFFER_INIT;
	reset_context_interface(i);

    }

}
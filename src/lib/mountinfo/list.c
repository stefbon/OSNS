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

#include <sys/sysmacros.h>

#include "libosns-log.h"
#include "libosns-list.h"

#include "list.h"
#include "monitor.h"
#include "utils.h"

int add_mountentry(struct mount_monitor_s *monitor, struct mountentry_s *me)
{
    struct list_header_s *header=&monitor->mountentries;
    int diff=1;
    struct list_element_s *list=get_list_tail(header);

    /* walk back starting at the last and compare */

    while (list) {
    	struct mountentry_s *prev=(struct mountentry_s *)((char *) list - offsetof(struct mountentry_s, list));

	if (prev->mountid < me->mountid) {

	    diff=-1;
	    break;

	} else if (prev->mountid == me->mountid) {

	    diff=0;
	    prev->generation=me->generation;
	    break;

	}

	diff=1;
	list=get_prev_element(list);

    }

    if (diff==-1) {

	add_list_element_after(header, list, &me->list);

    } else if (diff==0) {

	(* me->free)(me);

    } else {

	add_list_element_first(header, &me->list);

    }

    return diff;

}

struct mountentry_s *find_mountentry(struct mount_monitor_s *monitor, unsigned int offset, unsigned char flags)
{
    struct list_header_s *header=&monitor->mountentries;
    struct list_element_s *list=get_list_head(header);
    struct mountentry_s *me=NULL;

    signal_lock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);

    /* walk back starting at the last and compare */

    while (list) {

	me=(struct mountentry_s *)((char *) list - offsetof(struct mountentry_s, list));

	if ((me->mountid>=offset)) {

	    if ((me->mountid > offset) && (flags & FIND_MOUNTENTRY_FLAG_EXACT)) {

		me=NULL;

	    } else {

		signal_lock_flag(monitor->signal, &me->status, MOUNTENTRY_STATUS_LOCK);
		me->refcount++;
		signal_unlock_flag(monitor->signal, &me->status, MOUNTENTRY_STATUS_LOCK);

	    }

	    break;

	}

	list=get_next_element(list);
	me=NULL;

    }

    signal_unlock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);
    return me;

}

/* move mountentries with an "older" generation to the removed list */

void process_mountentries(struct mount_monitor_s *monitor)
{
    struct list_header_s *header=&monitor->mountentries;
    struct list_element_s *list=get_list_head(header);

    while (list) {
	struct mountentry_s *me=(struct mountentry_s *)((char *) list - offsetof(struct mountentry_s, list));
	struct list_element_s *next=get_next_element(list);

	if (me->generation < monitor->generation) {

	    /* this mount entry not found in the current generation */

	    remove_list_element(&me->list);
	    if ((* monitor->update)(MOUNTMONITOR_ACTION_REMOVED, me)==0) {

		add_list_element_last(&monitor->removedentries, &me->list);

	    } else {

		(* me->free)(me);

	    }

	} else if (me->found==monitor->generation) {

	    int result=(* monitor->update)(MOUNTMONITOR_ACTION_ADDED, me);

	}

	list=next;

    }

}

static void clear_mountentry_list(struct list_header_s *h)
{
    struct list_element_s *list=remove_list_head(h);

    while (list) {
	struct mountentry_s *me=(struct mountentry_s *)((char *) list - offsetof(struct mountentry_s, list));

	(* me->free)(me);
	list=remove_list_head(h);

    }
}

void clear_mountentry_lists(struct mount_monitor_s *monitor)
{
    clear_mountentry_list(&monitor->mountentries);
    clear_mountentry_list(&monitor->removedentries);
}

void browse_mountentries(struct mount_monitor_s *monitor, void (* cb)(struct mountentry_s *me, void *ptr), void *ptr)
{
    struct list_element_s *list=NULL;

    signal_lock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);
    list=get_list_head(&monitor->mountentries);

    while (list) {
        struct mountentry_s *me=(struct mountentry_s *)((char *) list - offsetof(struct mountentry_s, list));

        (* cb)(me, ptr);
        list=get_next_element(list);

    }

}
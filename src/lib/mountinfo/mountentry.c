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
#include <fcntl.h>

#include "libosns-log.h"
#include "libosns-list.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"

#include "monitor.h"
#include "list.h"
#include "utils.h"

void free_mountentry_data(struct mountentry_s *me)
{
    if (me->mountpoint) free_unescaped_string(me->mountpoint);
    if (me->rootpath) free_unescaped_string(me->rootpath);
    if (me->fs) free_unescaped_string(me->fs);
    if (me->source) free_unescaped_string(me->source);
    if (me->options) free_unescaped_string(me->options);
}

void free_mountentry(struct mountentry_s *me)
{
    struct mount_monitor_s *monitor=me->monitor;

    signal_lock_flag(monitor->signal, &me->status, MOUNTENTRY_STATUS_LOCK);

    if (me->refcount==0) {

	/* safe to free and not required to unlock (cause it's freed this is not even possible )*/

	free_mountentry_data(me);
	free(me);
	return;

    }

    /* set bit to free for releasing threads to free it after releasing it ...
	note that the process which calls this function has already taken care this mountentry is
	not part of any list ... so the releasing thread does not have to do that and that's a good thing
	since that thread is not aware of that */

    me->status |= MOUNTENTRY_STATUS_FREE;
    signal_unlock_flag(monitor->signal, &me->status, MOUNTENTRY_STATUS_LOCK);
}

void release_mountentry(struct mountentry_s *me)
{
    struct mount_monitor_s *monitor=me->monitor;

    signal_lock_flag(monitor->signal, &me->status, MOUNTENTRY_STATUS_LOCK);

    me->refcount--;

    if ((me->refcount==0) && (me->status & MOUNTENTRY_STATUS_FREE)) {

	free_mountentry_data(me);
	free(me);
	return;

    }

    signal_unlock_flag(monitor->signal, &me->status, MOUNTENTRY_STATUS_LOCK);

}

struct mountentry_s *create_mountentry(struct mount_monitor_s *monitor, struct mountentry_s *init)
{
    struct mountentry_s *me=malloc(sizeof(struct mountentry_s));

    if (me) {

	memset(me, 0, sizeof(struct mountentry_s));
	memcpy(me, init, sizeof(struct mountentry_s)); /* all variables and pointers */

	me->found=monitor->generation;
	me->generation=monitor->generation;
	me->refcount=0;
	me->status=0;
	get_current_time_system_time(&me->created);
	me->monitor=monitor;
	me->flags=0;
	init_list_element(&me->list, NULL);
	me->free=free_mountentry;

    }

    return me;

}

void get_location_path_mountpoint_mountentry(struct fs_location_path_s *path, struct mountentry_s *me)
{
    set_location_path(path, 'c', me->mountpoint);
}
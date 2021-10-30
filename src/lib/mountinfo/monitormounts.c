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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <dirent.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>

#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "log.h"

#include "main.h"
#include "misc.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "threads.h"

#include "mountinfo.h"
#include "monitor.h"
#include "monitormounts.h"

#include "fuse/network.h"
#include "discover/discover.h"

#define ACCEPTED_MOUNT_TYPE_SOURCE				1
#define ACCEPTED_MOUNT_TYPE_FS					2

#define _ADD_MOUNTINFO_FLAG_INCLUDE_FS				1
#define _ADD_MOUNTINFO_FLAG_EXCLUDE_FS				2
#define _ADD_MOUNTINFO_FLAG_INCLUDE_SOURCE			4
#define _ADD_MOUNTINFO_FLAG_EXCLUDE_SOURCE			8

struct accepted_mounts_s {
    unsigned char		flags;
    struct list_element_s	list;
    unsigned char		type;
    unsigned int		size;
    char			buffer[];
};

static struct bevent_s *bevent=NULL;
static struct list_header_s accepted_mounts=INIT_LIST_HEADER;
static unsigned char default_ignore_flag=0;

static int update_mountinfo(uint64_t generation, struct mountentry_s *(*next_me)(struct mountentry_s *me, uint64_t generation, unsigned char type), void *data, unsigned char flags)
{
    struct mountentry_s *me=NULL;
    unsigned int error=0;

    logoutput("update_mountinfo: generation %li", generation);

    me=next_me(me, generation, MOUNTLIST_ADDED);

    while (me) {
	struct workspace_mount_s *workspace=NULL;

	if (me->fs==NULL || me->source==NULL || me->mountpoint==NULL) goto next1;

	lock_workspaces();

	workspace=get_next_workspace_mount(NULL);

	while (workspace) {

	    if (strcmp(me->mountpoint, workspace->mountpoint.path)==0) {
		struct system_dev_s dev=SYSTEM_DEV_INIT;

		get_dev_system_stat(&workspace->inodes.rootinode.stat, &dev);
		if (get_unique_system_dev(&dev)==0) set_dev_system_stat(&workspace->inodes.rootinode.stat, &me->dev);
		break;

	    }

	    workspace=get_next_workspace_mount(workspace);

	}

	if (workspace) {

	    (* workspace->mountevent)(workspace, WORKSPACE_MOUNT_EVENT_MOUNT);

	} else {

    	    if (flags & MOUNTMONITOR_FLAG_INIT) {

		if (strcmp(me->source, "network@osns.net")==0) {

		    /* this will trigger the mount monitor, which will call this function ... interesting .. */

		    logoutput("update_mountinfo: umount %s", me->mountpoint);
		    umount2(me->mountpoint, MNT_DETACH);

		}

	    }

	}

	unlock_workspaces();

	next1:
	me=next_me(me, generation, MOUNTLIST_ADDED);

    }

    logoutput("update_mountinfo: look at removed mounts");

    me=next_me(NULL, generation, MOUNTLIST_REMOVED);

    while (me) {

	logoutput("update_mountinfo: removed %s:%s at %s", me->source, me->fs, me->mountpoint);
	next2:
	me=next_me(me, generation, MOUNTLIST_REMOVED);

    }

    return 1;

}

static unsigned char ignore_mountinfo (char *source, char *fs, char *path, void *data)
{
    struct list_element_s *list=get_list_head(&accepted_mounts, 0);
    int result=-1;

    if (default_ignore_flag==0) default_ignore_flag = ADD_MOUNTINFO_FLAG_INCLUDE;

    while (list) {
	struct accepted_mounts_s *mount=(struct accepted_mounts_s *)((char *) list - offsetof(struct accepted_mounts_s, list));

	if (mount->type==ACCEPTED_MOUNT_TYPE_SOURCE) {

	    if (strlen(source)==mount->size && memcmp(source, mount->buffer, mount->size)==0) {

		result =  (mount->flags & ADD_MOUNTINFO_FLAG_INCLUDE) ? 0 : 1;
		break;

	    }

	} else if (mount->type==ACCEPTED_MOUNT_TYPE_FS) {

	    if (strlen(fs)==mount->size && memcmp(fs, mount->buffer, mount->size)==0) {

		result = (mount->flags & ADD_MOUNTINFO_FLAG_INCLUDE) ? 0 : 1;
		break;

	    }

	}

	list=get_next_element(list);

    }

    if (result==-1) result=(default_ignore_flag & ADD_MOUNTINFO_FLAG_INCLUDE) ? 0 : 1;

    // logoutput("ignore_mountinfo: source %s fs %s path %s result %i", source, fs, path, result);

    return result;
}

static void add_mountinfo_common(char *data, unsigned char type, unsigned char flags)
{
    unsigned int len=0;
    struct accepted_mounts_s *mount=NULL;
    unsigned char tmp=0;

    if (data==NULL || flags==0) return;
    if ((flags & ADD_MOUNTINFO_FLAG_INCLUDE) && (flags & ADD_MOUNTINFO_FLAG_EXCLUDE)) {

	logoutput_warning("add_mountinfo_common: both exclude and include flags defined: cannot define both..ignoring");
	return;

    } else if ((flags & ADD_MOUNTINFO_FLAG_INCLUDE)==0 && (flags & ADD_MOUNTINFO_FLAG_EXCLUDE)==0) {

	logoutput_warning("add_mountinfo_common: one exclude and include flag has to be defined..ignoring");
	return;

    }

    if (flags & ADD_MOUNTINFO_FLAG_INCLUDE) {

	if (default_ignore_flag & ADD_MOUNTINFO_FLAG_INCLUDE) {

	    logoutput_warning("add_mountinfo_common: cannot use include and exclude flags at the same time..ignoring");
	    return;

	}

	tmp=ADD_MOUNTINFO_FLAG_EXCLUDE;

    } else if (flags & ADD_MOUNTINFO_FLAG_EXCLUDE) {

	if (default_ignore_flag & ADD_MOUNTINFO_FLAG_EXCLUDE) {

	    logoutput_warning("add_mountinfo_common: cannot use include and exclude flags at the same time..ignoring");
	    return;

	}

	tmp=ADD_MOUNTINFO_FLAG_INCLUDE;

    }

    len=strlen(data);
    mount=malloc(sizeof(struct accepted_mounts_s) + len);

    if (mount) {

	mount->type=type;
	mount->flags=flags;
	mount->size=len;
	memcpy(mount->buffer, data, len);
	add_list_element_last(&accepted_mounts, &mount->list);

	default_ignore_flag |= tmp;

	logoutput("add_mountinfo_common: add value %s as %s", (type==ACCEPTED_MOUNT_TYPE_SOURCE) ? "source" : "fs", data, (flags & ADD_MOUNTINFO_FLAG_EXCLUDE) ? "exclude" : "include");

    }

}

void add_mountinfo_source(char *source, unsigned char flags)
{
    add_mountinfo_common(source, ACCEPTED_MOUNT_TYPE_SOURCE, flags);
}

void add_mountinfo_fs(char *fs, unsigned char flags)
{
    add_mountinfo_common(fs, ACCEPTED_MOUNT_TYPE_FS, flags);
}

int add_mountinfo_watch(struct beventloop_s *loop)
{

    if (! loop) loop=get_mainloop();
    bevent=open_mountmonitor();

    if (bevent) {

	set_updatefunc_mountmonitor(update_mountinfo);
	set_ignorefunc_mountmonitor(ignore_mountinfo);
	set_threadsqueue_mountmonitor(NULL);

	if (add_bevent_beventloop(bevent)==0) {

    	    logoutput_info("add_mountinfo_watch: mountinfo fd %i added to eventloop", get_bevent_unix_fd(bevent));

	    /* read the mountinfo to initialize */

	    (* bevent->btype.fd.cb)(0, NULL, NULL);
	    return 0;

	} else {

	    logoutput_warning("add_mountinfo_watch: unable to add mountinfo fd %i to eventloop", get_bevent_unix_fd(bevent));

	}

    } else {

	logoutput_warning("add_mountinfo_watch: unable to open mountmonitor");

    }

    return -1;

}

void remove_mountinfo_watch()
{

    if (bevent) {
	struct list_element_s *list=NULL;

	remove_bevent(bevent);
	close_mountmonitor();

	while ((list=get_list_head(&accepted_mounts, SIMPLE_LIST_FLAG_REMOVE))) {
	    struct accepted_mounts_s *mount=(struct accepted_mounts_s *)((char *) list - offsetof(struct accepted_mounts_s, list));

	    free(mount);

	}

    }

}

void init_mountinfo_once()
{
    init_list_header(&accepted_mounts, SIMPLE_LIST_TYPE_EMPTY, 0);
}

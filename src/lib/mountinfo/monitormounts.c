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

struct accepted_mounts_s {
    struct list_element_s	list;
    unsigned char		type;
    unsigned int		size;
    char			buffer[];
};

static struct bevent_s bevent;
static struct list_header_s accepted_mounts=INIT_LIST_HEADER;

static int update_mountinfo(unsigned long generation, struct mountentry_s *(*next_me)(void **index, unsigned long generation, unsigned char type), void *data)
{
    struct mountentry_s *entry=NULL;
    struct workspace_mount_s *workspace=NULL;
    void *index2=NULL;
    unsigned int error=0;

    logoutput("update_mountinfo: generation %li", generation);

    entry=next_me(&index2, generation, MOUNTLIST_ADDED);
    if (entry->fs==NULL || entry->source==NULL || entry->mountpoint==NULL) goto next;

    while (entry) {
	void *index2=NULL;
	unsigned int hash=0;
	struct fuse_user_s *user=NULL;
	struct simple_lock_s rlock;

	init_rlock_users_hash(&rlock);
	lock_users_hash(&rlock);

	user=get_next_fuse_user(&index2, &hash);

	while (user) {
	    struct list_element_s *list=NULL;

	    list=get_list_head(&user->workspaces, 0);

	    while (list) {

		workspace=(struct workspace_mount_s *)(((char *)list) - offsetof(struct workspace_mount_s, list));

		if (strcmp(entry->mountpoint, workspace->mountpoint.path)==0) {

		    if (workspace->inodes.rootinode.st.st_dev==0) workspace->inodes.rootinode.st.st_dev=makedev(entry->major, entry->minor);
		    break;

		}

		list=get_next_element(list);
		workspace=NULL;

	    }

	    if (workspace) break;
	    user=get_next_fuse_user(&index2, &hash);

	}

	if (workspace && workspace->syncdate.tv_sec==0) {

	    if (workspace->type==WORKSPACE_TYPE_NETWORK) {

		logoutput("update_mountinfo: found network workspace on %s", workspace->mountpoint.path);
		get_net_services(&workspace->syncdate, install_net_services_cb, (void *) workspace);

	    }

	}

	next:
	entry=next_me(&index2, generation, MOUNTLIST_ADDED);

    }

    return 1;

}

static unsigned char ignore_mountinfo (char *source, char *fs, char *path, void *data)
{
    struct list_element_s *list=get_list_head(&accepted_mounts, 0);

    while (list) {
	struct accepted_mounts_s *mount=(struct accepted_mounts_s *)((char *) list - offsetof(struct accepted_mounts_s, list));

	if (mount->type==ACCEPTED_MOUNT_TYPE_SOURCE) {

	    if (strlen(source)==mount->size && memcmp(source, mount->buffer, mount->size)==0) return 0;

	} else if (mount->type==ACCEPTED_MOUNT_TYPE_FS) {

	    if (strlen(fs)==mount->size && memcmp(fs, mount->buffer, mount->size)==0) return 0;

	}

	list=get_next_element(list);

    }

    return 1;
}

void add_mountinfo_source(char *source)
{
    unsigned int len=0;
    struct accepted_mounts_s *mount=NULL;

    if (source==NULL) return;

    len=strlen(source);
    mount=malloc(sizeof(struct accepted_mounts_s) + len);

    if (mount) {

	mount->type=ACCEPTED_MOUNT_TYPE_SOURCE;
	mount->size=len;
	memcpy(mount->buffer, source, len);
	add_list_element_last(&accepted_mounts, &mount->list);

    }

}

void add_mountinfo_fs(char *fs)
{
    unsigned int len=0;
    struct accepted_mounts_s *mount=NULL;

    if (fs==NULL) return;

    len=strlen(fs);
    mount=malloc(sizeof(struct accepted_mounts_s) + len);

    if (mount) {

	mount->type=ACCEPTED_MOUNT_TYPE_FS;
	mount->size=len;
	memcpy(mount->buffer, fs, len);
	add_list_element_last(&accepted_mounts, &mount->list);

    }

}

int add_mountinfo_watch(struct beventloop_s *loop, unsigned int *error)
{
    init_bevent(&bevent);
    if (! loop) loop=get_mainloop();

    if (open_mountmonitor(&bevent, error)==0) {

	set_updatefunc_mountmonitor(update_mountinfo);
	set_ignorefunc_mountmonitor(ignore_mountinfo);
	set_threadsqueue_mountmonitor(NULL);

	if (add_to_beventloop(bevent.fd, EPOLLPRI, bevent.cb, NULL, &bevent, loop)) {

    	    logoutput_info("add_mountinfo_watch: mountinfo fd %i added to eventloop", bevent.fd);

	    /* read the mountinfo to initialize */
	    (* bevent.cb)(0, NULL, 0);
	    return 0;

	} else {

	    logoutput_info("add_mountinfo_watch: unable to add mountinfo fd %i to eventloop", bevent.fd);
	    *error=EIO;

	}

    } else {

	logoutput_info("add_mountinfo_watch: unable to open mountmonitor");

    }

    return -1;

}

void remove_mountinfo_watch()
{
    struct list_element_s *list=NULL;

    remove_bevent_from_beventloop(&bevent);
    close_mountmonitor();

    while ((list=get_list_head(&accepted_mounts, SIMPLE_LIST_FLAG_REMOVE))) {
	struct accepted_mounts_s *mount=(struct accepted_mounts_s *)((char *) list - offsetof(struct accepted_mounts_s, list));

	free(mount);

    }

}


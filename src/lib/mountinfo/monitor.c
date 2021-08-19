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
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include <strings.h>

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <fcntl.h>

#include <glib.h>

#undef LOGGING

#include "log.h"
#include "list.h"
#include "eventloop.h"
#include "threads.h"

#include "mountinfo.h"
#include "monitor.h"
#include "utils.h"

#define MOUNT_MONITOR_FLAG_INIT			1
#define MOUNTINFO_FILE "/proc/self/mountinfo"

struct mountinfo_monitor_s {
    int 					(*update) (uint64_t generation_id, struct mountentry_s *(*next) (struct mountentry_s *m, uint64_t g, unsigned char type), void *data, unsigned char flags);
    unsigned char 				(*ignore) (char *source, char *fs, char *path, void *data);
    struct bevent_s 				*bevent;
    pthread_mutex_t				mutex;
    pthread_t					threadid;
    unsigned char				changed;
    void 					*threadsqueue;
    void					*data;
    unsigned int				flags;
    struct simple_locking_s 			locking;
};

/* added, removed and keep mount entries */

static struct list_header_s removed_mounts;
static struct list_header_s current_mounts_m;
static struct list_header_s current_mounts_d;
static struct mountinfo_monitor_s monitor;
static void process_mountinfo_event(int fd, void *data, struct event_s *event);

void free_mountentry(struct mountentry_s *me)
{

    if (me) {

	if (me->mountpoint) free(me->mountpoint);
	if (me->fs) free(me->fs);
	if (me->source) free(me->source);
	if (me->options) free(me->options);
	if (me->rootpath) free(me->rootpath);
	free(me);

    }

}

int lock_mountlist_read(struct simple_lock_s *lock)
{
    init_simple_readlock(&monitor.locking, lock);
    return simple_lock(lock);
}

int lock_mountlist_write(struct simple_lock_s *lock)
{
    init_simple_writelock(&monitor.locking, lock);
    return simple_lock(lock);
}

int unlock_mountlist(struct simple_lock_s *lock)
{
    return simple_unlock(lock);
}

/*dummy callbacks*/

static unsigned char dummy_ignore(char *source, char *fs, char *path, void *data)
{
    return 0;
}

static int dummy_update(uint64_t generation, struct mountentry_s *(*next) (struct mountentry_s *me, uint64_t generation, unsigned char type), void *data, unsigned char flags)
{
    return 1;
}

struct bevent_s *open_mountmonitor()
{
    int fd=-1;
    struct bevent_s *bevent=NULL;

    init_simple_locking(&monitor.locking);

    /* current_mounts_m: list of mounts with same order as provided by the kernel (==mountinfo) */
    init_list_header(&current_mounts_m, SIMPLE_LIST_TYPE_EMPTY, NULL);
    /* current_mounts_d: list of mounts ordered to the major:minor value */
    init_list_header(&current_mounts_d, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&removed_mounts, SIMPLE_LIST_TYPE_EMPTY, NULL);

    monitor.update=dummy_update;
    monitor.ignore=dummy_ignore;
    monitor.bevent=NULL;
    pthread_mutex_init(&monitor.mutex, NULL);
    monitor.changed=0;
    monitor.threadid=0;
    monitor.data=NULL;
    monitor.threadsqueue=NULL;
    monitor.flags=0;

    fd=open(MOUNTINFO_FILE, O_RDONLY);

    if (fd==-1) {

    	logoutput_error("open_mountmonitor: unable to open file %s", MOUNTINFO_FILE);
	goto error;

    }

    bevent=create_fd_bevent(NULL, process_mountinfo_event, NULL);
    if (bevent==NULL) goto error;

    set_bevent_unix_fd(bevent, fd);
    set_bevent_watch(bevent, "u");
    monitor.bevent=bevent;

    logoutput("open_mountmonitor: fd %i", fd);
    return bevent;

    error:

    if (bevent) remove_bevent(bevent);
    if (fd>0) close(fd);
    return NULL;
}

void set_updatefunc_mountmonitor(update_cb_t cb)
{
    if (cb) monitor.update=cb;
}

void set_ignorefunc_mountmonitor(ignore_cb_t cb)
{
    if (cb) monitor.ignore=cb;
}

void set_threadsqueue_mountmonitor(void *ptr)
{
    if (ptr) monitor.threadsqueue=ptr;
}

void set_userdata_mountmonitor(void *data)
{
    if (data) monitor.data=data;
}

void close_mountmonitor()
{

    if (monitor.bevent) {

	int fd=get_bevent_unix_fd(monitor.bevent);
	if (fd>=0) {

	    close(fd);
	    set_bevent_unix_fd(monitor.bevent, -1);

	}

	monitor.bevent=NULL;

    }

    clear_simple_locking(&monitor.locking);

}

/* walk through the current mounts to get the added */

struct mountentry_s *get_next_mountentry_added(struct mountentry_s *me, uint64_t generation)
{
    struct list_element_s *list=NULL;

    if (me) {

	list=&me->list_m;
	list=get_next_element(list);
	me=NULL;

    } else {

	list=get_list_head(&current_mounts_m, 0);

    }

    while (list) {

	me=(struct mountentry_s *) ((char *) list - offsetof(struct mountentry_s, list_m));

	logoutput("get_next_mountentry_added: mount %s:%s at %s (generation=%li found=%li)", me->source, me->fs, me->mountpoint, (unsigned long) me->generation, (unsigned long) me->found);

	if (me->generation==generation && me->found==generation) break;
	list=get_next_element(list);
	me=NULL;

    }

    return me;

}

static void add_mount_removed(struct mountentry_s *mountentry)
{
    add_list_element_last(&removed_mounts, &mountentry->list_d);
}

static void clear_removed_mounts()
{
    struct list_element_s *list=get_list_head(&removed_mounts, SIMPLE_LIST_FLAG_REMOVE);

    logoutput("clear_removed_mounts");

    while (list) {

	struct mountentry_s *me=(struct mountentry_s *) ((char *) list - offsetof(struct mountentry_s, list_d));
	free_mountentry(me);
	list=get_list_head(&removed_mounts, SIMPLE_LIST_FLAG_REMOVE);

    }

}

struct mountentry_s *get_next_mountentry_removed(struct mountentry_s *me)
{
    struct list_element_s *list=NULL;
    struct list_element_s *next=NULL;

    if (me) {

	list=&me->list_m;
	next=get_next_element(list);
	me=NULL;

    } else {

	next=get_list_head(&removed_mounts, 0);

    }

    return (next) ? (struct mountentry_s *) ((char *) next - offsetof(struct mountentry_s, list_m)) : NULL;
}

struct mountentry_s *get_next_mountentry_current(struct mountentry_s *me)
{
    struct list_element_s *list=NULL;
    struct list_element_s *next=NULL;

    if (me) {

	list=&me->list_m; 
	next=get_next_element(list);

    } else {

	next=get_list_head(&current_mounts_m, 0);

    }

    return (next) ? (struct mountentry_s *) ((char *) next - offsetof(struct mountentry_s, list_m)) : NULL;
}

struct mountentry_s *get_next_mountentry(struct mountentry_s *me, uint64_t generation, unsigned char type)
{

    if (type==MOUNTLIST_CURRENT) {

	return get_next_mountentry_current(me);

    } else if (type==MOUNTLIST_ADDED) {

	return get_next_mountentry_added(me, generation);

    } else if (type==MOUNTLIST_REMOVED) {

	return get_next_mountentry_removed(me);

    }

    return NULL;

}

/* read one line from mountinfo */

static struct mountentry_s *read_mountinfo_values(char *buffer, unsigned int size, uint64_t generation)
{
    int mountid=0;
    int parentid=0;
    int major=0;
    int minor=0;
    char *mountpoint=NULL;
    char *root=NULL;
    char *source=NULL;
    char *options=NULL;
    char *fs=NULL;
    char *sep=NULL;
    char *pos=NULL;
    struct mountentry_s *me=NULL;
    unsigned int left=size;
    int error=0;
    char tmp[256];
    unsigned int len=0;

    pos=buffer;

    if (sscanf(pos, "%i %i %i:%i", &mountid, &parentid, &major, &minor) != 4) {

        logoutput_error("read_mountinfo_values: error sscanf, reading %s", pos);
	goto dofree;

    }

    /* determine the pos in the buffer by creating the first part self using snprintf */

    len=snprintf(tmp, 256, "%i %i %i:%i", mountid, parentid, major, minor);
    pos+=len;
    left-=len;

    /* root */

    sep=memchr(pos, '/', left);
    if (! sep) goto dofree;
    left-=(unsigned int) (sep-pos);
    pos=sep;

    sep=memchr(pos, ' ', left);
    if (! sep) goto dofree;
    *sep='\0';
    root=g_strcompress(pos); /* unescape */
    if (! root) goto dofree;
    *sep=' ';
    left-=(unsigned int) (sep-pos);
    pos=sep;

    /* mountpoint */

    sep=memchr(pos, '/', left);
    if (! sep) goto dofree;
    left-=(unsigned int) (sep-pos);
    pos=sep;
    sep=memchr(pos, ' ', left);
    if (! sep) goto dofree;

    *sep='\0';
    mountpoint=g_strcompress(pos); /* unescape */
    if (! mountpoint) goto dofree;
    *sep=' ';
    left-=(unsigned int) (sep-pos);
    pos=sep;

    /* skip rest here, and start at the seperator - where filesystem, source and options can be found */

    sep=strstr(pos, " - ");
    if ( ! sep ) goto dofree;
    sep+=3;
    left-=(unsigned int) (sep-pos);
    pos=sep;

    /* filesystem */

    sep=memchr(pos, ' ', left);
    if ( ! sep ) goto dofree;
    *sep='\0';
    fs=g_strcompress(pos);
    if (! fs) goto dofree;
    *sep=' ';
    sep++;

    left-=(unsigned int) (sep-pos);
    pos=sep;

    /* source */

    sep=memchr(pos, ' ', left);

    if (!sep) {

	sep=buffer + size - 1;

    } else {

	*sep='\0';

    }

    if (strcmp(pos, "/dev/root")==0) {

	source=get_real_root_device(major, minor);

    } else {

	source=g_strcompress(pos);

    }

    if (! source) goto dofree;
    if ((*monitor.ignore) (source, fs, mountpoint, monitor.data)==1) goto dofree;
    left-=(unsigned int) (sep-pos);

    /* options */

    if (left>1) {

	*sep=' ';
	pos=sep+1;
	left--;

	sep=memchr(pos, ' ', left);

	if (sep) {

	    *sep='\0';
	    options=g_strcompress(pos);
	    *sep=' ';

	} else {

	    options=g_strcompress(pos);

	}

	if (! options) goto dofree;

    }

    /* get a new mountinfo */

    logoutput("read_mountinfo_values: found %s:%s at %s major:minor %i:%i", source, fs, mountpoint, major, minor);

    me=malloc(sizeof(struct mountentry_s));
    if (me==NULL) goto dofree;

    me->found=generation;
    me->generation=generation;
    me->mountpoint=mountpoint;
    me->rootpath=root;
    me->fs=fs;
    me->source=source;
    me->options=options;
    me->major=major;
    me->minor=minor;
    me->flags=0;

    init_list_element(&me->list_m, NULL);
    init_list_element(&me->list_d, NULL);

    me->mountid=mountid;
    me->parentid=parentid;
    me->parent=NULL;

    if (strcmp(fs, "autofs")==0) {

        if (strstr(options, "indirect")) {

	    me->flags|=MOUNTENTRY_FLAG_AUTOFS_INDIRECT;

	} else {

	    me->flags|=MOUNTENTRY_FLAG_AUTOFS_DIRECT;

	}

    }

    return me;

    dofree:

    if (mountpoint) free(mountpoint);
    if (root) free(root);
    if (fs) free(fs);
    if (source) free(source);
    if (options) free(options);
    if (me) free(me);
    return NULL;

}

static int add_mountentry_sorted_d(struct list_header_s *header, struct mountentry_s *me)
{
    int diff=-1;
    struct list_element_s *walk=get_list_tail(header, 0);
    dev_t dev=makedev(me->major, me->minor);
    uint64_t cnt=0;

    /* walk back starting at the last and compare */

    while (walk) {
    	struct mountentry_s *prev=(struct mountentry_s *)((char *) walk - offsetof(struct mountentry_s, list_d));
	dev_t devprev=makedev(prev->major, prev->minor);

	cnt++;
	if (cnt > (3 * header->count / 2)) {

	    logoutput("read_mountinfo_values: loop detected..");
	    break;

	}

	/* look for the mounentry which major:minor is less (or equal) to get a sorted list */

	if (devprev <= dev) {

	    if (devprev==dev) {

		prev->generation=me->generation; /* prev also found this batch */
		diff=0;

	    } else {

		diff=1;

	    }

	    break;

	}

	walk=get_prev_element(walk);

    }

    if (diff==-1 || diff==1) {

	if (walk) {

	    add_list_element_after(header, walk, &me->list_d);

	} else {

	    add_list_element_first(header, &me->list_d);

	}

    }

    return diff;

}

static int get_mountlist(uint64_t generation, unsigned int init)
{
    int error=0;
    FILE *fp=NULL;
    unsigned int size= 2 * PATH_MAX; /* theoretically big enough ?? */
    char buffer[size];
    int ctr_added=0;

    logoutput("get_mountlist");

    fp=fopen(MOUNTINFO_FILE, "r");

    if (fp==NULL) {

	logoutput("get_mountlist: error opening %s", MOUNTINFO_FILE);
	error=errno;
	goto out;

    }

    memset(buffer, 0, size);

    while (fgets(buffer, size - 1, fp)) {

	size_t len=0;
	char *sep=NULL;
	struct mountentry_s *me=NULL;

	sep=memchr(buffer, '\n', size);
	if (sep) *sep='\0';
	len=strnlen(buffer, size);

	me=read_mountinfo_values(buffer, len, generation);

	if (me) {
	    int diff=0;

	    diff=add_mountentry_sorted_d(&current_mounts_d, me);

	    if (init) {

		/* add always to current mounts */

		add_list_element_last(&current_mounts_m, &me->list_m);
		ctr_added++;

	    } else {

		if (diff==-1 || diff==1) {

		    add_list_element_last(&current_mounts_m, &me->list_m);
		    ctr_added++;

		} else {

		    free_mountentry(me);

		}

	    }

	}

	memset(buffer, 0, size);

    }

    fclose(fp);
    out:
    return (error>0) ? -error : ctr_added;

}

void handle_change_mounttable(unsigned char init)
{
    uint64_t generation=0;
    unsigned int error=0;
    int ctr_added=0;
    struct simple_lock_s wlock;
    struct list_element_s *list=NULL;

    logoutput("handle_change_mounttable");

    lock_mountlist_write(&wlock);

    generation=generation_id();
    increase_generation_id();

    /* get a new list and compare */

    ctr_added=get_mountlist(generation, init);
    if (ctr_added<0) {

	error=abs(ctr_added);
	logoutput("handle_change_mounttable: error %i getting list of mounts (%s)", error, strerror(error));
	goto unlock;

    }

    /* look for "old" mountentries: removed */

    logoutput("handle_change_mounttable: comparing current and new mounts");

    list=get_list_head(&current_mounts_d, 0);

    while (list) {
	struct mountentry_s *me=(struct mountentry_s *)((char *) list - offsetof(struct mountentry_s, list_d));

	if (me->generation<generation) {

	    /* this mountentry is not found this batch */

	    remove_list_element(list);
    	    add_list_element_last(&removed_mounts, list);

	} else {

	    check_mounted_by_autofs(me);

	}

        list=get_next_element(list);

    }

    if (ctr_added>0 || removed_mounts.count>0) {
	unsigned char flags=0;

	if (init) {

	    flags |= MOUNTMONITOR_FLAG_INIT;

	}

	logoutput("handle_change_mounttable: run the cb");

	if ((*monitor.update) (generation, get_next_mountentry, monitor.data, flags)==1) {

	    logoutput("handle_change_mounttable: clear removed mounts");

	    clear_removed_mounts();

	}

    }

    logoutput("handle_change_mounttable: unlock");

    unlock:

    unlock_mountlist(&wlock);

}

static void thread_process_mountinfo(void *ptr)
{

    logoutput("thread_process_mountinfo");

    pthread_mutex_lock(&monitor.mutex);

    process:

    monitor.changed=0;
    monitor.threadid=pthread_self();
    pthread_mutex_unlock(&monitor.mutex);

    handle_change_mounttable(! (monitor.flags & MOUNT_MONITOR_FLAG_INIT));

    pthread_mutex_lock(&monitor.mutex);
    if (monitor.changed==1) goto process;
    monitor.threadid=0;
    pthread_mutex_unlock(&monitor.mutex);

}

/* process an event the mountinfo */

static void process_mountinfo_event(int fd, void *data, struct event_s *events)
{

    logoutput("process_mountinfo_event");

    pthread_mutex_lock(&monitor.mutex);

    if (monitor.threadid>0) {

	/* there is already a thread processing */
	monitor.changed=1;
	logoutput("process_mountinfo_event: mount monitor is already active: signal this");

    } else if (events==NULL) {

	handle_change_mounttable(1);
	monitor.flags|=MOUNT_MONITOR_FLAG_INIT;

    } else {

	/* get a thread to do the work */

	work_workerthread(NULL, 0, thread_process_mountinfo, NULL, NULL);

    }

    pthread_mutex_unlock(&monitor.mutex);

}

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

#include <sys/fanotify.h> 

#include "libosns-misc.h"
#include "libosns-log.h"
#include "lib/system/path.h"
#include "lib/system/stat.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

static const char *backend_name="fsnotify_subsystem:fanotify";

struct fsnotify_backend_s {
    struct bevent_s				*bevent;
    struct shared_signal_s			*signal;
    struct list_header_s			watches;
};

struct fsnotify_watch_s {
    uint64_t					mask;
    struct directory_s				*directory;
    struct list_header_s			subscribers;
};

static struct directory_s *create_fswatch_path(struct fs_location_path_s *path)
{
    struct directory_s *directory=&root;

    

struct fswatch_s *add_watch_fsnotify(struct bevent_subsystem_s *subsys, struct fsnotify_subscriber_s *subscriber, struct fs_location_path_s *path, unsigned int mask)
{
    struct fswatch_s *watch=NULL;
    struct fswatch_subscription_s *fsws=NULL;
    struct list_element_s *list=NULL;
    struct fsnotify_backend_s *backend=(struct fsnotify_backend_s *) subsys->buffer;
    struct shared_signal_s *signal=backend->signal;
    struct fswatch_path_s search;

    logoutput("add_watch_fsnotify");

    memset(&search, 0, sizeof(struct fswatch_path_s));
    set_location_path(&search->path, 'p', (void *) path);
    search->mask=mask;

    signal_set_flag(signal, &backend->flags, FSNOTIFY_BACKEND_FLAG_LOCK_WATCHES);

    /* check first a watch does exist already for this path for this subscriber */

    watch=search_create_fswatch_backend(backend, search);
    if (watch==NULL) {

	signal_unset_flag(signal, &backend->flags, FSNOTIFY_BACKEND_FLAG_LOCK_WATCHES);
	goto failed;

    }

    /* there is a watch */

    if (watch->subscriptions.count==0) {

	fsws=add_watch_subscription(watch, subscriber, mask);

    } else {
	struct list_element_s *list=get_list_head(&watch->subscriptions, 0);

	while (list) {

	    fsws=(struct fswatch_subscription_s *)((char *)list - offsetof(struct struct fswatch_subscription_s, list_w));
	    if (get_subscriber_watch(fsws)==subscriber) break;
	    list=get_next_element(list);
	    fsws=NULL;

	}

	if (fsws==NULL) fsws=add_watch_subscription(watch, subscriber, mask);

    }

    if (fsws) {

	if ((watch->mask & mask)!=mask) {

	    watch->mask |= mask;
	    reset_fswatch_backend(watch);

	}

    } else {

	signal_unset_flag(signal, &backend->flags, FSNOTIFY_BACKEND_FLAG_LOCK_WATCHES);
	goto failed;

    }

    signal_unset_flag(signal, &backend->flags, FSNOTIFY_BACKEND_FLAG_LOCK_WATCHES);
    return watch;

    failed:

    logoutput_warning("add_watch_fsnotify: unable to created a watch");
    return NULL;

}

static int start_btimer_backend(struct bevent_subsystem_s *subsys)
{
    struct btimer_backend_s *backend=(struct fsno_backend_s *) subsys->buffer;
    struct bevent_s *bevent=backend->bevent;
    int result=-1;

    if (subsys->flags & BEVENT_SUBSYSTEM_FLAG_START) return 0;

    if (bevent) {
	int fd=-1;

#ifdef __linux__

	fd=timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);

#endif

	if (fd<0) {

	    logoutput_warning("start_btimer_subsystem: unable to open timer fd - error %i (%s)", errno, strerror(errno));
	    goto out;

	}

	(* bevent->ops->set_unix_fd)(bevent, fd);
	set_bevent_watch(bevent, "i"); /* watch incoming events */
	add_bevent_watch(bevent);
	result=0;
	subsys->flags |= BEVENT_SUBSYSTEM_FLAG_START;

    };

    out:
    return result;

}

static void stop_btimer_backend(struct bevent_subsystem_s *subsys)
{
    struct btimer_backend_s *backend=(struct btimer_backend_s *) subsys->buffer;
    struct bevent_s *bevent=backend->bevent;

    if (subsys->flags & BEVENT_SUBSYSTEM_FLAG_STOP) return;

    if (bevent) {
	int fd=(* bevent->ops->get_unix_fd)(bevent);

	if (fd>=0) {

	    close(fd);
	    (* bevent->ops->set_unix_fd)(bevent, -1);

	}

	subsys->flags |= BEVENT_SUBSYSTEM_FLAG_STOP;

    }

}

static void clear_btimer_backend(struct bevent_subsystem_s *subsys)
{
    struct btimer_backend_s *backend=(struct btimer_backend_s *) subsys->buffer;
    struct shared_signal_s *signal=backend->signal;
    struct list_element_s *list=NULL;

    if (subsys->flags & BEVENT_SUBSYSTEM_FLAG_CLEAR) return;

    /* clear bevent */

    if (backend->bevent) {
	struct bevent_s *bevent=backend->bevent;

	remove_list_element(&bevent->list);
	(* bevent->ops->free_bevent)(&bevent);
	backend->bevent=NULL;

    }

    /* clear timers (if any) */

    signal_set_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK);

    list=get_list_head(&backend->timers, SIMPLE_LIST_FLAG_REMOVE);
    while (list) {
	struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	(* timer->cb)(timer->id, timer->ptr, BTIMER_FLAG_REMOVED);
	free(timer);
	list=get_list_head(&backend->timers, SIMPLE_LIST_FLAG_REMOVE);

    }

    signal_unset_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK);
    subsys->flags |= BEVENT_SUBSYSTEM_FLAG_CLEAR;

}

static struct bevent_subsystem_ops_s fsnotify_backend_ops = {
    .start_subsys				= start_fsnotify_backend,
    .stop_subsys				= stop_fsnotify_backend,
    .clear_subsys				= clear_fsnotify_backend,
};

/*
    with linux create a fsnotify subsys using fanotify
*/

int init_fsnotify_backend(struct beventloop_s *loop, struct bevent_subsystem_s *subsys)
{
    unsigned int size=sizeof(struct fsnotify_backend_s);
    struct fsnotify_backend_s *backend=NULL;
    struct bevent_s *bevent=NULL;

    if (subsys==NULL || subsys->size < size) return size;
    backend=(struct fsnotify_backend_s *) subsys->buffer;
    memset(backend, 0, subsys->size);

    backend->signal=loop->signal;
    subsys->name=backend_name;
    subsys->ops=&fsnotify_backend_ops;
    backend->bevent=NULL;
    init_list_header(&backend->watches, SIMPLE_LIST_TYPE_EMPTY, NULL);
    backend->id=1;

    bevent=create_fd_bevent(loop, fsnotivy_bevent_cb, (void *) backend);

    if (bevent==NULL) {

	logoutput_warning("init_timerfd_subsystem: unable to alloc bevent");
	goto failed;

    }

    backend->bevent=bevent;
    return 0;

    failed:

    if (bevent) {

	free_bevent(&bevent);
	backend->bevent=NULL;

    }

    return -1;

}

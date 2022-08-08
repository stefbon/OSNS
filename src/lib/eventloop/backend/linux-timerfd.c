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

#include <time.h>
#include <sys/timerfd.h> 

#include "libosns-misc.h"
#include "libosns-log.h"
#include "libosns-system.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

static pthread_mutex_t					timer_id_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *backend_name="btimer_subsystem:timerfd";
static struct system_timespec_s zero_time=SYSTEM_TIME_INIT;

/* private struct: no need to make this public, the only thing important to the context is the calling/execute of the cb at the right time */

#define BTIMER_BACKEND_FLAG_LOCK_TIMERS			1
#define BTIMER_BACKEND_FLAG_LOCK_ADJUST			4

struct btimer_backend_s {
    unsigned int					flags;
    struct bevent_s					*bevent;
    struct shared_signal_s				*signal;
    struct list_header_s				timers;
    struct list_element_s				*current;
    unsigned int					id;
};

struct btimer_s {
    struct system_timespec_s				expire;
    struct list_element_s 				list;
    void						*ptr;
    unsigned int					id;
    void						(* cb)(unsigned int id, void *ptr, unsigned char flags);
};

static struct btimer_s dummy_timer;

static void adjust_btimer_backend(struct btimer_backend_s *backend, struct system_timespec_s *expire)
{
    struct bevent_s *bevent=backend->bevent;

    if (bevent) {

#ifdef __linux__

	int fd=(* bevent->ops->get_unix_fd)(bevent);

	if (fd>=0) {
	    struct shared_signal_s *signal=backend->signal;

	    struct itimerspec new;

	    new.it_interval.tv_sec=0;
	    new.it_interval.tv_nsec=0;

	    if (expire) {

		new.it_value.tv_sec=expire->st_sec;
		new.it_value.tv_nsec=expire->st_nsec;

	    } else {

		/* this will disable the timer */

		new.it_value.tv_sec=0;
		new.it_value.tv_nsec=0;

	    }

	    signal_lock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_ADJUST);

	    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new, NULL)==-1) {

		if (expire) {

		    logoutput_warning("adjust_btimer_subsystem: error %i setting timer to value %lu:%i (%s)", errno, expire->st_sec, expire->st_nsec, strerror(errno));

		} else {

		    logoutput_warning("adjust_btimer_subsystem: error %i disabling timer (%s)", errno, strerror(errno));

		}

	    }

	    signal_unlock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_ADJUST);

	}

#else

	logoutput_warning("adjust_btimer_subsystem: timer not supported");

#endif


    }

}

static unsigned int get_unique_timer_id(struct btimer_backend_s *backend)
{
    unsigned int id=0;

    pthread_mutex_lock(&timer_id_mutex);
    id=backend->id;
    backend->id++;
    pthread_mutex_unlock(&timer_id_mutex);
    return id;
}

unsigned int add_timer_timerfd(struct bevent_subsystem_s *subsys, struct system_timespec_s *expire, void (* cb)(unsigned int id, void *ptr, unsigned char flags), void *ptr)
{
    struct btimer_s *timer=NULL;
    struct system_timespec_s current=SYSTEM_TIME_INIT;
    struct btimer_backend_s *backend=(struct btimer_backend_s *) subsys->buffer;
    struct shared_signal_s *signal=backend->signal;

    logoutput_debug("add_timer_timerfd");

    if (expire) {

	get_current_time_system_time(&current);
	if (system_time_test_earlier(expire, &current)>=0) return 0; /* expire in the past ... this is useless */

    }

    timer=malloc(sizeof(struct btimer_s));
    if (timer==NULL) return 0;
    memset(timer, 0, sizeof(struct btimer_s));
    init_list_element(&timer->list, NULL);

    timer->ptr=ptr;
    timer->id=get_unique_timer_id(backend);
    timer->cb=cb;

    if (expire) {

	copy_system_time(&timer->expire, expire);

    } else {

	logoutput_debug("add_timer_timerfd: id %i no expire", timer->id);

	set_system_time(&timer->expire, 0, 0);
	signal_lock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);
	add_list_element_first(&backend->timers, &timer->list);
	signal_unlock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);
	return timer->id;

    }

    /* list with timer might be in use by another add timer,
	or a timer event happened and the system is scanning for expired timers */

    signal_lock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

    if (backend->current) {
	struct list_element_s *list=backend->current;

	logoutput_debug("add_timer_timerfd: id %i while", timer->id);

	while (list) {
	    struct btimer_s *tmp=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	    if (system_time_test_earlier(expire, &tmp->expire)>0) break; /* stop at the first next timer which is later */
	    list=get_next_element(list);

	}

	if (list) {

	    logoutput_debug("add_timer_timerfd: id %i list", timer->id);

	    add_list_element_before(&backend->timers, list, &timer->list);

	    if (list==backend->current) {

		adjust_btimer_backend(backend, expire);
		backend->current=&timer->list;

	    }

	    logoutput_debug("add_timer_timerfd: id %i list-post", timer->id);

	} else {

	    logoutput_debug("add_timer_timerfd: id %i last", timer->id);

	    add_list_element_last(&backend->timers, &timer->list);

	}

    } else {

	add_list_element_last(&backend->timers, &timer->list);
	adjust_btimer_backend(backend, expire);
	backend->current=&timer->list;

    }

    logoutput_debug("add_timer_timerfd: id %i unset", timer->id);

    signal_unlock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

    logoutput_debug("add_timer_timerfd: id %i out", timer->id);
    return timer->id;
}

static unsigned char move_timer_in_list_earlier(struct btimer_backend_s *backend, struct list_element_s *list, struct system_timespec_s *expire)
{
    struct list_element_s *prev=get_prev_element(list);
    unsigned int count=0;
    unsigned char first=0;

    while (prev) {
	struct btimer_s *timer=(struct btimer_s *)((char *) prev - offsetof(struct btimer_s, list));

	/* if dealing with an inactive timer quit searching */

	if (prev==backend->current) {

	    if (system_time_test_earlier(&timer->expire, expire)>0) {

		/* earlier than the first active timer
		    the modified becomes the first active timer*/

		if (count>0) {

		    remove_list_element(list);
		    add_list_element_before(&backend->timers, prev, list);

		}

		first=1;
		backend->current=list;

	    } else {

		if (count>0) {

		    remove_list_element(list);
		    add_list_element_after(&backend->timers, prev, list);

		}

	    }

	    break;

	} else if (system_time_test_earlier(&timer->expire, &zero_time)==0) {

	    /* previous timers are inactive ... */

	    if (count>0) {

		remove_list_element(list);
		add_list_element_after(&backend->timers, prev, list);

	    }

	    backend->current=list;
	    first=1;
	    break;

	} else if (system_time_test_earlier(&timer->expire, expire)>=0) {

	    /* previous is earlier: stop */

	    if (count>0) {

		remove_list_element(list);
		add_list_element_after(&backend->timers, prev, list);

	    }

	    break;


	}

	prev=get_prev_element(prev);
	count++;

    }

    return first;

}

static void move_timer_in_list_later(struct btimer_backend_s *backend, struct list_element_s *list, struct system_timespec_s *expire)
{
    struct list_element_s *next=get_next_element(list);
    unsigned int count=0;

    while (next) {
	struct btimer_s *timer=(struct btimer_s *)((char *) next - offsetof(struct btimer_s, list));

	if (system_time_test_earlier(expire, &timer->expire)>=0) break;
	next=get_next_element(next);
	count++;

    }

    if (count>0) {

	remove_list_element(list);

	if (next) {

	    add_list_element_before(&backend->timers, next, list);

	} else {

	    add_list_element_last(&backend->timers, list);

	}

    }

}

#define ACTION_TIMER_OP_MODIFY						1
#define ACTION_TIMER_OP_REMOVE						2

static void modify_timer_timerfd_common(struct bevent_subsystem_s *subsys, struct btimer_s *timer, unsigned char op, int compareresult, struct system_timespec_s *expire)
{
    struct btimer_backend_s *backend=(struct btimer_backend_s *) subsys->buffer;
    struct list_element_s *list=&timer->list;
    unsigned char isfirst=0;

    if (op==ACTION_TIMER_OP_REMOVE) {

	if (list==backend->current) {
	    struct list_element_s *next=get_next_element(list);

	    if (next) {
		struct btimer_s *tmp=(struct btimer_s *)((char *) next - offsetof(struct btimer_s, list));

		expire=&tmp->expire;

	    } else {

		expire=NULL;

	    }

	    isfirst=1;
	    backend->current=next;

	}

	remove_list_element(list);
	free(timer);

    } else if (op==ACTION_TIMER_OP_MODIFY) {

	if (compareresult>0) {

	    isfirst=move_timer_in_list_earlier(backend, list, expire);

	} else {

	    move_timer_in_list_later(backend, list, expire);

	}

    }

    if (isfirst) adjust_btimer_backend(backend, expire);

}

static unsigned char modify_timer_timerfd_id(struct bevent_subsystem_s *subsys, unsigned int id, unsigned char op, struct system_timespec_s *expire)
{
    struct btimer_backend_s *backend=(struct btimer_backend_s *) subsys->buffer;
    struct shared_signal_s *signal=backend->signal;
    struct list_element_s *list=NULL;
    unsigned char found=0;
    int compareresult=0;

    logoutput_debug("modify_timer_timerfd_id: id %i", id);

    signal_lock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

    /* first find the timer ... this maybe slow, a hashtable is better than */

    list=get_list_head(&backend->timers, 0);

    while (list) {
	struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	if (timer->id==id) {

	    if (op==ACTION_TIMER_OP_MODIFY) {

		compareresult=system_time_test_earlier(expire, &timer->expire);
		if (compareresult==0) list=NULL; /* if no difference than nothing has to be changed */

	    }

	    break;

	}

	list=get_next_element(list);

    }

    signal_unlock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

    if (list) {
	struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	found=1;
	modify_timer_timerfd_common(subsys, timer, op, compareresult, expire);

    }

    return found;

}

unsigned char modify_timer_timerfd(struct bevent_subsystem_s *subsys, unsigned int id, void *ptr, struct system_timespec_s *expire)
{
    unsigned char found=1;

    if (ptr) {
	struct btimer_s *timer=(struct btimer_s *) ptr;
	int compareresult=system_time_test_earlier(expire, &timer->expire);

	if (compareresult) modify_timer_timerfd_common(subsys, timer, ACTION_TIMER_OP_MODIFY, compareresult, expire);

    } else {

	found=modify_timer_timerfd_id(subsys, id, ACTION_TIMER_OP_MODIFY, expire);

    }

    return found;
}

unsigned char remove_timer_timerfd(struct bevent_subsystem_s *subsys, unsigned int id, void *ptr)
{

    if (ptr) {
	struct btimer_s *timer=(struct btimer_s *) ptr;

	modify_timer_timerfd_common(subsys, timer, ACTION_TIMER_OP_REMOVE, 0, NULL);
	return 1;

    }

    return modify_timer_timerfd_id(subsys, id, ACTION_TIMER_OP_REMOVE, NULL);
}

static void launch_btimer_cb(void *ptr)
{
    struct btimer_s *timer=(struct btimer_s *) ptr;
    logoutput_debug("launch_btimer_cb: id %i", timer->id);
    (* timer->cb)(timer->id, timer->ptr, BTIMER_FLAG_EXPIRED);
}

/* walk back to the */

static void thread2handle_expirations(void *ptr)
{
    struct bevent_s *bevent=(struct bevent_s *) ptr;
    struct list_element_s *list=NULL;
    struct btimer_backend_s *backend=(struct btimer_backend_s *) bevent->ptr;
    struct shared_signal_s *signal=backend->signal;
    struct system_timespec_s current=SYSTEM_TIME_INIT;

    signal_lock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

    list=backend->current;

    while (list) {
	struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));
	struct list_element_s *next=NULL;

	/* only look at the the timers which have expired
	    and since it's an ordered (in time) linked list quit at the first timer which is not expired */

	get_current_time_system_time(&current);
	if (system_time_test_earlier(&timer->expire, &current)<0) break;
	work_workerthread(NULL, -1, launch_btimer_cb, (void *) timer, NULL);
	list=get_next_element(list);

    }

    /* if there is a first element in the list adjust the system timer */

    if (list) {
	struct btimer_s *tmp=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	backend->current=list;
	adjust_btimer_backend(backend, &tmp->expire);

    } else {

	backend->current=NULL;
	adjust_btimer_backend(backend, NULL);

    }

    signal_unlock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

}

static void timer_bevent_cb(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{

    if (signal_is_data(arg)) {

#ifdef __linux__

	int fd=(* bevent->ops->get_unix_fd)(bevent);
	int result=-1;
	uint64_t expirations;

	result=read(fd, &expirations, sizeof(uint64_t));

	if (result==sizeof(uint64_t)) {

	    if (expirations>0) {

		work_workerthread(NULL, -1, thread2handle_expirations, (void *) bevent, NULL);

	    }

	}

#endif

    }

}

static int start_btimer_backend(struct bevent_subsystem_s *subsys)
{
    struct btimer_backend_s *backend=(struct btimer_backend_s *) subsys->buffer;
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

	    adjust_btimer_backend(backend, NULL);

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

    signal_lock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);

    list=get_list_head(&backend->timers, SIMPLE_LIST_FLAG_REMOVE);
    while (list) {
	struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	(* timer->cb)(timer->id, timer->ptr, BTIMER_FLAG_REMOVED);
	free(timer);
	list=get_list_head(&backend->timers, SIMPLE_LIST_FLAG_REMOVE);

    }

    signal_unlock_flag(signal, &backend->flags, BTIMER_BACKEND_FLAG_LOCK_TIMERS);
    subsys->flags |= BEVENT_SUBSYSTEM_FLAG_CLEAR;

}

static struct bevent_subsystem_ops_s timer_backend_ops = {
    .start_subsys				= start_btimer_backend,
    .stop_subsys				= stop_btimer_backend,
    .clear_subsys				= clear_btimer_backend,
};

/* with linux create a timer subsys using timerfd
*/

int init_timerfd_backend(struct beventloop_s *loop, struct bevent_subsystem_s *subsys)
{
    unsigned int size=sizeof(struct btimer_backend_s);
    struct btimer_backend_s *backend=NULL;
    struct bevent_s *bevent=NULL;

    if (subsys==NULL || subsys->size < size) return size;
    backend=(struct btimer_backend_s *) subsys->buffer;
    memset(backend, 0, sizeof(struct btimer_backend_s));

    backend->signal=loop->signal;
    subsys->name=backend_name;
    subsys->ops=&timer_backend_ops;
    backend->flags=0;
    backend->bevent=NULL;
    init_list_header(&backend->timers, SIMPLE_LIST_TYPE_EMPTY, NULL);
    backend->current=NULL;
    backend->id=1;

    bevent=create_fd_bevent(loop, (void *) backend);

    if (bevent==NULL) {


	logoutput_warning("init_timerfd_subsystem: unable to alloc bevent");
	goto failed;

    }

    set_bevent_cb(bevent, (BEVENT_FLAG_CB_DATAAVAIL | BEVENT_FLAG_CB_ERROR | BEVENT_FLAG_CB_CLOSE), timer_bevent_cb);
    backend->bevent=bevent;

    /* dummy timer as first in list
	to make sure there is always a prev ... */
    memset(&dummy_timer, 0, sizeof(struct btimer_s));
    set_system_time(&dummy_timer.expire, 0, 0);
    init_list_element(&dummy_timer.list, NULL);
    dummy_timer.ptr=NULL;
    dummy_timer.id=0;
    add_list_element_first(&backend->timers, &dummy_timer.list);

    return 0;

    failed:

    if (bevent) {

	free_bevent(&bevent);
	backend->bevent=NULL;

    }

    return -1;

}

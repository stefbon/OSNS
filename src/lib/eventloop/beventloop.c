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

#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>

#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <syslog.h>

#include "global-defines.h"

#include "eventloop.h"
#include "misc.h"
#include "log.h"

struct btimer_s {
    struct list_element_s 				list;
    struct beventloop_s					*loop;
    void						*ptr;
    unsigned int					id;
    void						(* cb)(unsigned int id, void *ptr);
};

static struct beventloop_s beventloop_main = {.flags=BEVENTLOOP_FLAG_MAIN};

static gboolean glib_fd_cb(gpointer ptr)
{
    struct bevent_s *bevent=(struct bevent_s *) ptr;
    struct event_s event;
    int fd=get_bevent_unix_fd(bevent);

    logoutput_debug("glib_fd_cb: fd %i", fd);

    event.events.glib_revents=bevent->ltype.glib.pollfd.revents;
    event.loop=get_eventloop_bevent(bevent);

    (* bevent->btype.fd.cb)(fd, bevent->ptr, &event);
    return G_SOURCE_CONTINUE;

}

static void close_bevent_glib(struct bevent_s *bevent)
{
    int fd=get_bevent_unix_fd(bevent);

    if (fd>=0) {

	close(fd);
	set_bevent_unix_fd(bevent, -1);

    }

}

static void close_bevent(struct bevent_s *bevent)
{
    (* bevent->close)(bevent);
}

struct bevent_s *create_fd_bevent(struct beventloop_s *eloop, void (* cb)(int fd, void *ptr, struct event_s *event), void *ptr)
{
    struct bevent_s *bevent=NULL;

    logoutput_debug("create_fd_bevent");

    if (eloop==NULL) eloop=&beventloop_main;

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	bevent=(struct bevent_s *) g_source_new(&eloop->type.glib.funcs, sizeof(struct bevent_s));

	if (bevent) {

	    bevent->flags=0;
	    bevent->btype.fd.cb=cb;
	    bevent->ptr=ptr;
	    bevent->close=close_bevent_glib;

	    pthread_mutex_lock(&eloop->mutex);
	    add_list_element_last(&eloop->bevents, &bevent->list);
	    pthread_mutex_unlock(&eloop->mutex);

	    g_source_ref((GSource *) &bevent->ltype.glib.source);
	    bevent->ltype.glib.pollfd.events = G_IO_HUP | G_IO_ERR; /* defaults */
	    bevent->ltype.glib.pollfd.fd=-1;

	    bevent->flags |= BEVENT_FLAG_CREATE;

	}

    }

    return bevent;

}


struct beventloop_s *get_eventloop_bevent(struct bevent_s *bevent)
{
    struct list_header_s *h=bevent->list.h;
    struct beventloop_s *loop=NULL;

    loop=(h) ? ((struct beventloop_s *)((char *) h - offsetof(struct beventloop_s, bevents))) : NULL;
    return loop;

}

struct bevent_s *get_next_bevent(struct beventloop_s *loop, struct bevent_s *bevent)
{
    struct list_element_s *list=NULL;

    if (bevent) {

	list=get_next_element(&bevent->list);

    } else {

	if (loop==NULL) loop=&beventloop_main;
	list=get_list_head(&loop->bevents, 0);

    }

    return ((list) ? (struct bevent_s *)((char *) list - offsetof(struct bevent_s, list)) : NULL);
}

void set_bevent_cb(struct bevent_s *bevent, void (* cb)(int fd, void *ptr, struct event_s *event))
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);

    if (loop) {

	if (loop->flags & BEVENTLOOP_FLAG_GLIB) bevent->btype.fd.cb=cb;

    }

}

void set_bevent_ptr(struct bevent_s *bevent, void *ptr)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);

    if (loop) {

	if (loop->flags & BEVENTLOOP_FLAG_GLIB) bevent->ptr=ptr;

    }

}

void set_bevent_unix_fd(struct bevent_s *bevent, int fd)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);

    if (loop) {

	if (loop->flags & BEVENTLOOP_FLAG_GLIB) {

	    logoutput_debug("set_bevent_unix_fd: fd %i", fd);
	    bevent->ltype.glib.pollfd.fd = (gint) fd;

	}

    } else {

	logoutput_warning("set_bevent_unix_fd: loop not set for bevent");

    }

}

int get_bevent_unix_fd(struct bevent_s *bevent)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);
    int fd=-1;

    if (loop) {

	if (loop->flags & BEVENTLOOP_FLAG_GLIB) {

	    fd=(int) bevent->ltype.glib.pollfd.fd;

	}

    } else {

	logoutput_warning("get_bevent_unix_fd: loop not set for bevent");

    }

    return fd;

}

short get_bevent_events(struct bevent_s *bevent)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);
    short events=0;

    if (loop) {

	if (loop->flags & BEVENTLOOP_FLAG_GLIB) {

	    events = (short ) bevent->ltype.glib.pollfd.events;

	}

    } else {

	logoutput_warning("get_bevent_events: loop not set for bevent");

    }

    return events;
}

static short get_glib_events_from_what(const char *what)
{
    short events=0;

    for (unsigned int i=0; i<strlen(what); i++) {

	if (strncmp(&what[i], "i", 1)==0) {

	    /* listen to incoming data to read */

	    events |= G_IO_IN | G_IO_PRI;

	} else if (strncmp(&what[i], "u", 1)==0) {

	    events |= G_IO_PRI;

	} else if (strncmp(&what[i], "o", 1)==0) {

	    events |= G_IO_OUT;

	} else {

	    logoutput_warning("get_glib_events_from_what: error what %.*s not supported", 1, &what[i]);

	}

    }

    return events;

}

int modify_bevent_watch(struct bevent_s *bevent, const char *what, short events)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);
    int result=-1;

    if (loop) {
	if (loop->flags & BEVENTLOOP_FLAG_GLIB) {

	    if (what) events=get_glib_events_from_what(what);

	    if (events==0) {

		/* remove bevent */

	    } else {

		events |= (G_IO_HUP | G_IO_ERR);

	        if (bevent->flags & BEVENT_FLAG_EVENTLOOP) {

		    /* added to eventloop already: change */

		    if (bevent->ltype.glib.pollfd.events != events) {

			/* as far as I can tell no other way to remove and add again */

			g_source_remove_poll((GSource *) &bevent->ltype.glib.source, &bevent->ltype.glib.pollfd);
			bevent->ltype.glib.pollfd.events = events;
			g_source_add_poll((GSource *) &bevent->ltype.glib.source, &bevent->ltype.glib.pollfd);
			result=0;

		    }

		} else {

		    /* not added to eventloop: */

		    bevent->ltype.glib.pollfd.events = events;
		    result=0;

		}

	    }

	}

    }

    return result;

}

void set_bevent_watch(struct bevent_s *bevent, const char *what)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);

    if (loop) {

	if (loop->flags & BEVENTLOOP_FLAG_GLIB) {

	    /* default flags */

	    bevent->ltype.glib.pollfd.events = G_IO_HUP | G_IO_ERR;

	    for (unsigned int i=0; i<strlen(what); i++) {

		if (strncmp(&what[i], "i", 1)==0) {

		    /* listen to incoming data to read */

		    bevent->ltype.glib.pollfd.events |= G_IO_IN | G_IO_PRI;

		} else if (strncmp(&what[i], "u", 1)==0) {

		    bevent->ltype.glib.pollfd.events |= G_IO_PRI;

		} else if (strncmp(&what[i], "o", 1)==0) {

		    bevent->ltype.glib.pollfd.events |= G_IO_OUT;

		} else {

		    logoutput_warning("set_bevent_watch: error what %.*s not supported", 1, &what[i]);

		}

	    }

	}

    }

}

int add_bevent_beventloop(struct bevent_s *bevent)
{
    int result=-1;
    struct beventloop_s *eloop=NULL;
    int fd=get_bevent_unix_fd(bevent);

    logoutput("add_bevent_beventloop: fd %i", fd);

    if (bevent==NULL) return -1;
    if ((bevent->flags & BEVENT_FLAG_EVENTLOOP)) return 0;
    eloop=get_eventloop_bevent(bevent);
    if (eloop==NULL) return -1;

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	if (bevent->ltype.glib.pollfd.fd<0) {

	    logoutput_warning("add_bevent_beventloop: cannot add fd %i (less than zero)", fd);
	    goto failed;

	} else if (bevent->ltype.glib.pollfd.events<=0) {

	    logoutput_warning("add_bevent_beventloop: poll events not positive (%i)", bevent->ltype.glib.pollfd.events);
	    goto failed;

	}

	g_source_add_poll((GSource *) &bevent->ltype.glib.source, &bevent->ltype.glib.pollfd);
	g_source_set_callback((GSource *) &bevent->ltype.glib.source, glib_fd_cb, (gpointer) bevent, NULL);
	g_source_attach((GSource *) &bevent->ltype.glib.source, g_main_loop_get_context(eloop->type.glib.loop));
	result=0;
	bevent->flags |= BEVENT_FLAG_EVENTLOOP;

    }

    return result;

    failed:
    logoutput_warning("add_bevent_beventloop: failed to add event to eventloop");
    return -1;
}

void remove_bevent(struct bevent_s *bevent)
{

    if (bevent) {

	if (bevent->flags |= BEVENT_FLAG_EVENTLOOP) {

	    g_source_remove_poll((GSource *) &bevent->ltype.glib.source, &bevent->ltype.glib.pollfd);
	    g_source_destroy((GSource *) bevent);
	    bevent->flags &= ~BEVENT_FLAG_EVENTLOOP;

	}

    }

}

void free_bevent(struct bevent_s **p_bevent)
{
    struct bevent_s *bevent=*p_bevent;

    if (bevent) {

	remove_bevent(bevent);

	if (bevent->flags & BEVENT_FLAG_CREATE) {

	    remove_list_element(&bevent->list);
	    bevent->flags &= ~BEVENT_FLAG_CREATE;

	}

	g_source_unref((GSource *) &bevent->ltype.glib.source);
	*p_bevent=NULL;

    }

}

static uint32_t event_is_error_glib(struct event_s *event)
{
    return (uint32_t) (event->events.glib_revents & G_IO_ERR);
}

static uint32_t event_is_close_glib(struct event_s *event)
{
    return (uint32_t) (event->events.glib_revents & G_IO_HUP);
}

static uint32_t event_is_data_glib(struct event_s *event)
{
    return (uint32_t) (event->events.glib_revents & ( G_IO_IN | G_IO_PRI ));
}

static uint32_t event_is_buffer_glib(struct event_s *event)
{
    return (uint32_t) (event->events.glib_revents & G_IO_OUT);
}

static gboolean eloop_glib_prepare(GSource *source, gint *t)
{
    *t=-1;
    return FALSE;
}

static gboolean eloop_glib_check(GSource *source)
{
    struct bevent_s *bevent=(struct bevent_s *) source;
    gushort revents=bevent->ltype.glib.pollfd.revents;

    if (revents & (G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP)) return TRUE;
    return FALSE;
}

static gboolean eloop_glib_dispatch(GSource *source, GSourceFunc cb, gpointer ptr)
{
    return (* cb)(ptr);
}

static gboolean timer_glib_cb(gpointer ptr)
{
    struct btimer_s *timer=(struct btimer_s *) ptr;

    if (timer) {
	struct beventloop_s *eloop=timer->loop;

	write_lock_list_header(&eloop->timers, &eloop->mutex, &eloop->cond);
	remove_list_element(&timer->list);
	write_unlock_list_header(&eloop->timers, &eloop->mutex, &eloop->cond);

	(* timer->cb)(timer->id, timer->ptr);
	free(timer);

    }

    return FALSE;
}

unsigned int create_timer_eventloop(struct beventloop_s *eloop, struct timespec *timeout, void (* cb)(unsigned int id, void *ptr), void *ptr)
{
    struct btimer_s *timer=malloc(sizeof(struct btimer_s));
    unsigned int id=0;

    if (timer) {
	unsigned int interval=0;
	unsigned int tmp=0;

	memset(timer, 0, sizeof(struct btimer_s));
	init_list_element(&timer->list, NULL);

	/* tv_nsec is nanoseconds: 10 ^ -9
	    to get the milli seconds multiply divide that by 10 ^ 6 */

	tmp = (unsigned int)(timeout->tv_nsec / 1000000);
	interval = 1000 * timeout->tv_sec + tmp;

	GSource *source=g_timeout_source_new((guint) interval);

	if (source) {

	    g_source_set_callback(source, timer_glib_cb, (gpointer) timer, NULL);
	    g_source_attach(source, g_main_loop_get_context(eloop->type.glib.loop));
	    id=(unsigned int) g_source_get_id(source);
	    timer->id=id;
	    timer->ptr=ptr;
	    timer->loop=eloop;

	    write_lock_list_header(&eloop->timers, &eloop->mutex, &eloop->cond);
	    add_list_element_last(&eloop->timers, &timer->list);
	    write_unlock_list_header(&eloop->timers, &eloop->mutex, &eloop->cond);

	} else {

	    free(timer);

	}

    }

    return id;

}

void remove_timer_eventloop(struct beventloop_s *eloop, unsigned int id)
{
    struct list_element_s *list=NULL;

    if (g_source_remove((guint) id)) {

	logoutput("remove_timer_eventloop: found and removed timer %i from glib mainloop", id);

    } else {

	logoutput("remove_timer_eventloop: timer with id %i not found in glib mainloop", id);

    }

    write_lock_list_header(&eloop->timers, &eloop->mutex, &eloop->cond);

    list=get_list_head(&eloop->timers, 0);
    while (list) {
	struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	if (timer->id==id) {

	    remove_list_element(list);
	    free(timer);
	    break;

	}

	list=get_next_element(list);

    }

    write_unlock_list_header(&eloop->timers, &eloop->mutex, &eloop->cond);

}

static int _init_beventloop(struct beventloop_s *eloop)
{

    if ((eloop->flags & BEVENTLOOP_FLAG_GLIB)==0) {

	/* choose one .. */

	eloop->flags |= BEVENTLOOP_FLAG_GLIB;

    }

    init_list_header(&eloop->timers, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&eloop->bevents, SIMPLE_LIST_TYPE_EMPTY, NULL);
    pthread_mutex_init(&eloop->mutex, NULL);
    pthread_cond_init(&eloop->cond, NULL);

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	if (eloop->type.glib.loop==NULL) {

	    eloop->type.glib.loop=g_main_loop_new(NULL, 0);

	    if (eloop->type.glib.loop==NULL) {

		logoutput("_init_beventloop: not able to create glib mainloop");
		return -1;

	    }

	    eloop->type.glib.funcs.prepare=eloop_glib_prepare;
	    eloop->type.glib.funcs.check=eloop_glib_check;
	    eloop->type.glib.funcs.dispatch=eloop_glib_dispatch;
	    eloop->type.glib.funcs.finalize=NULL;

	    eloop->event_is_error=event_is_error_glib;
	    eloop->event_is_close=event_is_close_glib;
	    eloop->event_is_data=event_is_data_glib;
	    eloop->event_is_buffer=event_is_buffer_glib;

	}

    }

    return 0;

}

int init_beventloop(struct beventloop_s *eloop)
{
    int result=-1;

    if (eloop==NULL) eloop=&beventloop_main;

    if ((eloop->flags & BEVENTLOOP_FLAG_INIT)==0) {

	result=_init_beventloop(eloop);
	if (result==0) eloop->flags |= BEVENTLOOP_FLAG_INIT;

    } else {

	result=0;

    }

    return result;

}

int start_beventloop(struct beventloop_s *eloop)
{

    if (eloop==NULL) eloop=&beventloop_main;
    if (init_beventloop(eloop)==-1) goto error;
    if (eloop->flags & BEVENTLOOP_FLAG_RUNNING) return 0;

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	eloop->flags |= BEVENTLOOP_FLAG_RUNNING;
	g_main_loop_run(eloop->type.glib.loop);

    }

    out:
    return 0;

    error:
    logoutput("start_beventloop: error...");
    return -1;
}

void stop_beventloop(struct beventloop_s *eloop)
{

    if (!eloop) eloop=&beventloop_main;
    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	g_main_loop_quit(eloop->type.glib.loop);
	eloop->flags &= ~BEVENTLOOP_FLAG_RUNNING;

    }

}

void clear_beventloop(struct beventloop_s *eloop)
{

    if (!eloop) eloop=&beventloop_main;

    if ((eloop->flags & BEVENTLOOP_FLAG_INIT)==0) {

	/* not even initialized: there is nothing to clear ... */
	return;

    } else if ((eloop->flags & BEVENTLOOP_FLAG_RUNNING)) {

	/* do not clear a running loop */
	return;

    }

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	if (eloop->type.glib.loop) {

	    g_main_loop_unref(eloop->type.glib.loop);
	    eloop->type.glib.loop=NULL;

	}

    }

    if (eloop->timers.count>0) {
	struct list_element_s *list=NULL;

	gettimer:

	list=get_list_head(&eloop->timers, SIMPLE_LIST_FLAG_REMOVE);

	if (list) {
	    struct btimer_s *timer=(struct btimer_s *)((char *) list - offsetof(struct btimer_s, list));

	    g_source_remove((guint) timer->id);
	    free(timer);
	    list=NULL;
	    goto gettimer;

	}

    }

    pthread_mutex_destroy(&eloop->mutex);
    pthread_cond_destroy(&eloop->cond);

}

void free_beventloop(struct beventloop_s **p_loop)
{
    struct beventloop_s *eloop=*p_loop;

    if (eloop) {

	clear_beventloop(eloop);
	if (eloop->flags & BEVENTLOOP_FLAG_ALLOC) {

	    free(eloop);
	    *p_loop=NULL;

	}

    }

}

struct beventloop_s *get_mainloop()
{
    return &beventloop_main;
}

uint32_t signal_is_error(struct event_s *event)
{
    struct beventloop_s *loop=event->loop;
    return (* loop->event_is_error)(event);
}

uint32_t signal_is_close(struct event_s *event)
{
    struct beventloop_s *loop=event->loop;
    return (* loop->event_is_close)(event);
}

uint32_t signal_is_data(struct event_s *event)
{
    struct beventloop_s *loop=event->loop;
    return (* loop->event_is_data)(event);
}

uint32_t signal_is_buffer(struct event_s *event)
{
    struct beventloop_s *loop=event->loop;
    return (* loop->event_is_buffer)(event);
}

unsigned int printf_event_uint(struct event_s *event)
{
    return (unsigned int) (event->events.glib_revents);
}

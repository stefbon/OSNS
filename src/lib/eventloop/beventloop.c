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
    event.loop=bevent->loop;

    (* bevent->btype.fd.cb)(fd, bevent->ptr, &event);
    return G_SOURCE_CONTINUE;

}

static void close_bevent(struct bevent_s *bevent)
{
    struct beventloop_s *eloop=bevent->loop;

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {
	int fd=get_bevent_unix_fd(bevent);

	if (fd>=0) {

	    close(fd);
	    set_bevent_unix_fd(bevent, -1);

	}

    }

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
	    bevent->loop=eloop;
	    bevent->btype.fd.cb=cb;
	    bevent->ptr=ptr;
	    bevent->close=close_bevent;

	    g_source_ref((GSource *) &bevent->ltype.glib.source);

	}

    }

    return bevent;

}

void set_bevent_unix_fd(struct bevent_s *bevent, int fd)
{

    if (bevent->loop->flags & BEVENTLOOP_FLAG_GLIB) {

	logoutput_debug("set_bevent_unix_fd: fd %i", fd);

	bevent->ltype.glib.pollfd.fd= (gint) fd;

    }

}

int get_bevent_unix_fd(struct bevent_s *bevent)
{
    int fd=-1;

    if (bevent->loop->flags & BEVENTLOOP_FLAG_GLIB) {

	fd=(int) bevent->ltype.glib.pollfd.fd;

    }

    return fd;

}

void set_bevent_watch(struct bevent_s *bevent, const char *what)
{

    if (bevent->loop->flags & BEVENTLOOP_FLAG_GLIB) {

	/* default flags */

	bevent->ltype.glib.pollfd.events = G_IO_HUP | G_IO_ERR;

	if (strcmp(what, "incoming data")==0) {

	    bevent->ltype.glib.pollfd.events |= G_IO_IN | G_IO_PRI;

	} else if (strcmp(what, "urgent data")==0) {

	    bevent->ltype.glib.pollfd.events |= G_IO_PRI;

	} else if (strcmp(what, "outgoing data")==0) {

	    bevent->ltype.glib.pollfd.events |= G_IO_OUT;

	} else {

	    logoutput_warning("set_bevent_watch: error what %s not supported", what);

	}

    }

}

int add_bevent_beventloop(struct bevent_s *bevent)
{
    int result=-1;
    struct beventloop_s *eloop=NULL;
    int fd=get_bevent_unix_fd(bevent);

    logoutput_debug("add_bevent_beventloop: fd %i", fd);

    if (bevent==NULL || (bevent->flags & BEVENT_FLAG_EVENTLOOP)) return -1;

    if (bevent->loop==NULL) bevent->loop=&beventloop_main;
    eloop=bevent->loop;

    if (eloop->flags & BEVENTLOOP_FLAG_GLIB) {

	if (bevent->ltype.glib.pollfd.fd<0 || bevent->ltype.glib.pollfd.events<=0) return -1;

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
    g_source_destroy((GSource *) bevent);
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
    return (uint32_t) (event->events.glib_revents & G_IO_IN);
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

    if (revents & G_IO_IN || revents & G_IO_PRI || revents & G_IO_ERR || revents & G_IO_HUP) return TRUE;

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

    clear_beventloop(eloop);
    if (eloop->flags & BEVENTLOOP_FLAG_ALLOC) {

	free(eloop);
	*p_loop=NULL;

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
    (unsigned int) (event->events.glib_revents);
}

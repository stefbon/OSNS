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
#include <sys/wait.h>

#include "libosns-eventloop.h"
#include "libosns-misc.h"
#include "libosns-log.h"

#ifdef HAVE_GLIB2

#include <glib.h>

#define GLIB_FLAG_ALL					(G_IO_IN | G_IO_OUT | G_IO_HUP | G_IO_PRI | G_IO_ERR)

struct _bevent_glib_s {
    GSource				source;
    GPollFD				pollfd;
    struct beventloop_s                 *loop;
    struct list_element_s               list;
    struct bevent_s			common;
};

static gboolean glib_fd_cb(gpointer ptr)
{
    struct bevent_s *bevent=(struct bevent_s *) ptr;
    struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)((char *) bevent - offsetof(struct _bevent_glib_s, common));
    logoutput_debug("glib_fd_cb: events %u", tmp->pollfd.revents);
    if (queue_bevent_events(tmp->loop, tmp->pollfd.revents, bevent)) work_workerthread(NULL, 0, beventloop_process_events_thread, (void *) tmp->loop);
    return G_SOURCE_CONTINUE;

}

static int ctl_io_bevent_glib(struct beventloop_s *eloop, struct bevent_s *bevent, unsigned char op, unsigned int events)
{
    struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)((char *) bevent - offsetof(struct _bevent_glib_s, common));
    int result=-1;

    logoutput_debug("ctl_bevent_glib");
    events &= GLIB_FLAG_ALL;
    if (events==0 && op==BEVENT_CTL_MOD) return -1;

    if (op==BEVENT_CTL_DEL) {

	if (bevent->flags & BEVENT_FLAG_ENABLED) {

	    g_source_remove_poll((GSource *) &tmp->source, &tmp->pollfd);
	    bevent->flags &= ~BEVENT_FLAG_ENABLED;
	    tmp->pollfd.events = 0;
	    result=0;

	}

    } else if (op==BEVENT_CTL_MOD) {

	tmp->pollfd.events = events;
	result=0;

    } else if (op==BEVENT_CTL_ADD) {

	if ((bevent->flags & BEVENT_FLAG_ENABLED)==0) {

            logoutput_debug("ctl_bevent_glib: add events %u", events);

	    tmp->pollfd.events = events;
	    g_source_add_poll((GSource *) &tmp->source, &tmp->pollfd);
	    g_source_set_callback((GSource *) &tmp->source, glib_fd_cb, (gpointer) bevent, NULL);
	    g_source_attach((GSource *) &tmp->source, NULL);

	    bevent->flags |= BEVENT_FLAG_ENABLED;
	    result=0;

	} else {

	    logoutput_warning("ctl_bevent_glib: fd %i already added", tmp->pollfd.fd);

	}

    }

    return result;
}

static unsigned char set_property_glib(struct bevent_s *bevent, const char *what, unsigned char enable)
{
    return 0;
}

static gboolean eloop_glib_prepare(GSource *source, gint *t)
{
    *t=-1;
    return FALSE;
}

static gboolean eloop_glib_check(GSource *source)
{
    struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)source;
    gushort revents=tmp->pollfd.revents;
    if (revents & GLIB_FLAG_ALL) return TRUE;
    return FALSE;
}

static gboolean eloop_glib_dispatch(GSource *source, GSourceFunc cb, gpointer ptr)
{
    return (* cb)(ptr);
}

static GSourceFuncs glib_funcs = {
    .prepare			= eloop_glib_prepare,
    .check			= eloop_glib_check,
    .dispatch			= eloop_glib_dispatch,
    .finalize			= NULL,
};

static void free_bevent_glib(struct bevent_s **p_bevent)
{
    struct bevent_s *bevent=*p_bevent;

    if (bevent) {
	struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)((char *) bevent - offsetof(struct _bevent_glib_s, common));

	g_source_destroy((GSource *) &tmp->source);
	g_free((GSource *) &tmp->source);
	*p_bevent=NULL;

    }

}

static void set_bevent_unix_fd_glib(struct bevent_s *bevent, int fd)
{
    struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)((char *) bevent - offsetof(struct _bevent_glib_s, common));

    logoutput_debug("set_bevent_unix_fd_glib: fd %i", fd);
    tmp->pollfd.fd = (gint) fd;
}

static int get_bevent_unix_fd_glib(struct bevent_s *bevent)
{
    struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)((char *) bevent - offsetof(struct _bevent_glib_s, common));
    return (int) tmp->pollfd.fd;
}

static struct bevent_ops_s glib_bevent_ops = {
    .ctl_io_bevent			= ctl_io_bevent_glib,
    .set_property			= set_property_glib,
    .free_bevent			= free_bevent_glib,
    .get_unix_fd			= get_bevent_unix_fd_glib,
    .set_unix_fd			= set_bevent_unix_fd_glib,
};

static void start_beventloop_glib(struct beventloop_s *loop)
{
    struct _beventloop_glib_s *tmp=NULL;

    if (loop==NULL) return;
    tmp=&loop->backend.glib;
    (* loop->first_run)(loop);
    g_main_loop_run(tmp->loop);
}

static void stop_beventloop_glib(struct beventloop_s *loop, unsigned int signo)
{
    struct _beventloop_glib_s *tmp=NULL;
    struct shared_signal_s *signal=NULL;

    if (loop==NULL) return;
    tmp=&loop->backend.glib;
    signal=loop->signal;
    g_main_loop_quit(tmp->loop);

    signal_set_flag(signal, &loop->flags, BEVENTLOOP_FLAG_STOP);
}

static void clear_beventloop_glib(struct beventloop_s *loop)
{
    struct _beventloop_glib_s *tmp=NULL;
    struct shared_signal_s *signal=NULL;

    if (loop==NULL) return;
    tmp=&loop->backend.glib;
    signal=loop->signal;
    g_main_loop_unref(tmp->loop);
    tmp->loop=NULL;
    signal_set_flag(signal, &loop->flags, BEVENTLOOP_FLAG_CLEAR);
}

static int init_io_bevent_glib(struct beventloop_s *loop, struct bevent_s *bevent)
{
    struct _bevent_glib_s *tmp=(struct _bevent_glib_s *)((char *) bevent - offsetof(struct _bevent_glib_s, common));

    g_source_ref((GSource *) &tmp->source);
    tmp->pollfd.events 		= 0;
    tmp->pollfd.fd		=-1;
    bevent->ops			=&glib_bevent_ops;
    return 0;

}

struct bevent_s *create_io_bevent_glib(struct beventloop_s *loop)
{
    struct _bevent_glib_s *tmp=NULL;
    struct bevent_s *bevent=NULL;

    tmp=(struct _bevent_glib_s *) g_source_new(&glib_funcs, sizeof(struct _bevent_glib_s));

    if (tmp) {

	g_source_ref((GSource *) &tmp->source);
	tmp->pollfd.events = loop->BEVENT_INIT;
	tmp->pollfd.fd=-1;
	tmp->loop=loop;

	bevent=&tmp->common;
	init_bevent(bevent);
	bevent->ops=&glib_bevent_ops;
	bevent->flags=BEVENT_FLAG_CREATE;

    }

    return bevent;

}

static struct beventloop_ops_s glib_loop_ops = {
    .start_eventloop			= start_beventloop_glib,
    .stop_eventloop			= stop_beventloop_glib,
    .clear_eventloop			= clear_beventloop_glib,
    .create_bevent			= create_io_bevent_glib,
    .init_io_bevent			= init_io_bevent_glib,
};

void set_beventloop_glib(struct beventloop_s *loop)
{
    if (loop==NULL) return;
    loop->ops=&glib_loop_ops;
    loop->BEVENT_IN=G_IO_IN;
    loop->BEVENT_OUT=G_IO_OUT;
    loop->BEVENT_ERR=G_IO_ERR;
    loop->BEVENT_HUP=G_IO_HUP;
    loop->BEVENT_PRI=G_IO_PRI;
    loop->BEVENT_INIT=(loop->BEVENT_ERR | loop->BEVENT_HUP);
    loop->BEVENT_ET=0; /* edge triggered supported in gmainloop ? I think not */
}

int init_beventloop_glib(struct beventloop_s *loop)
{
    struct _beventloop_glib_s *tmp=NULL;

    logoutput("init_beventloop_glib");
    if (loop==NULL) return -1;

    tmp=&loop->backend.glib;
    tmp->loop=g_main_loop_new(NULL, 0);
    if (tmp->loop==NULL) return -1;
    init_list_header(&tmp->events, SIMPLE_LIST_TYPE_EMPTY, NULL);

    tmp->funcs.prepare=eloop_glib_prepare;
    tmp->funcs.check=eloop_glib_check;
    tmp->funcs.dispatch=eloop_glib_dispatch;
    tmp->funcs.finalize=NULL;

    set_beventloop_glib(loop);
    return 0;

}

#else

void set_beventloop_glib(struct beventloop_s *eloop)
{}

int init_beventloop_glib(struct beventloop_s *loop)
{
    return -1;
}

#endif

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

#include "libosns-misc.h"
#include "libosns-log.h"
#include "libosns-socket.h"

#include "beventloop.h"

void set_bevent_osns_socket(struct bevent_s *bevent, struct osns_socket_s *sock)
{

    logoutput_debug("set_bevent_osns_socket");

    if (sock) {

	bevent->sock=sock;
	sock->flags |= OSNS_SOCKET_FLAG_BEVENT;
	sock->event.bevent=bevent;

	/* some eventloops need to have the fd around */

#ifdef __linux__

	int fd=(* sock->get_unix_fd)(sock);
	(* bevent->ops->set_unix_fd)(bevent, fd);

#endif

    } else {

	bevent->sock=NULL;

#ifdef __linux__

	(* bevent->ops->set_unix_fd)(bevent, -1);

#endif

    }

}

void unset_bevent_osns_socket(struct bevent_s *bevent, struct osns_socket_s *sock)
{

    bevent->sock=NULL;

#ifdef __linux__
    (* bevent->ops->set_unix_fd)(bevent, -1);
#endif

    if (sock) {

	sock->flags &= ~OSNS_SOCKET_FLAG_BEVENT;
	sock->event.bevent=NULL;

    }

}

static void cb_bevent_dummy(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    logoutput_debug("cb_bevent_dummy: flag %u", flag);
}

void init_bevent(struct bevent_s *bevent)
{
    memset(bevent, 0, sizeof(struct bevent_s));
    init_list_element(&bevent->list, NULL);
}

struct bevent_s *create_fd_bevent(struct beventloop_s *eloop, void *ptr)
{
    struct bevent_s *bevent=NULL;

    logoutput_debug("create_fd_bevent");

    if (eloop==NULL) eloop=get_default_mainloop();
    bevent=(* eloop->ops->create_bevent)(eloop);

    if (bevent) {
	struct shared_signal_s *signal=eloop->signal;

	bevent->flags=(BEVENT_FLAG_CREATE);
	bevent->ptr=ptr;
	for (unsigned int i=0; i<5; i++) bevent->cb[i]=cb_bevent_dummy;
	set_system_time(&bevent->unblocked, 0, 0);
	init_list_element(&bevent->elist, NULL);

	signal_lock_flag(signal, &eloop->flags, BEVENTLOOP_FLAG_BEVENTS_LOCK);
	add_list_element_last(&eloop->bevents, &bevent->list);
	signal_unlock_flag(signal, &eloop->flags, BEVENTLOOP_FLAG_BEVENTS_LOCK);

    }

    return bevent;

}

struct beventloop_s *get_eventloop_bevent(struct bevent_s *bevent)
{
    struct list_header_s *h=NULL;
    struct beventloop_s *loop=NULL;

    // logoutput_debug("get_eventloop_bevent: bevent %s", ((bevent) ? "DEF" : "UNDEF"));

    h=bevent->list.h;

    // logoutput_debug("get_eventloop_bevent: h %s", ((h) ? "DEF" : "UNDEF"));

    loop=(h) ? ((struct beventloop_s *)((char *) h - offsetof(struct beventloop_s, bevents))) : NULL;
    return loop;

}

void clear_io_event(struct bevent_s *bevent, unsigned int code, struct bevent_argument_s *arg)
{
    struct beventloop_s *loop=arg->loop;

    logoutput_debug("clear_io_event: (tid %u) bevent code %u event code %u arg code %u", gettid(), bevent->event.code, code, arg->event.code);

    write_lock_list_element(&bevent->elist);
    bevent->event.code &= ~code;
    write_unlock_list_element(&bevent->elist);

}

/* remove bevent from the events list */

void remove_io_event(struct bevent_s *bevent)
{
    struct list_header_s *h=NULL;

    write_lock_list_element(&bevent->elist);

    bevent->event.code=0;
    h=bevent->elist.h;

    if (h) {

	write_lock_list_header(h);
        remove_list_element(&bevent->elist);
        write_unlock_list_header(h);

    }

    write_unlock_list_element(&bevent->elist);
}

void beventloop_process_events_thread(void *ptr)
{
    struct beventloop_s *loop=(struct beventloop_s *) ptr;
    struct list_element_s *list=NULL;

    processlist:

    write_lock_list_header(&loop->events);
    list=remove_list_head(&loop->events);
    write_unlock_list_header(&loop->events);

    if (list) {
        struct bevent_s *bevent=(struct bevent_s *)((char *) list - offsetof(struct bevent_s, elist));
	struct bevent_argument_s arg;

	/* bevent->event.code &= ~elist->event.code; */

	arg.loop=loop;
	arg.event.code=bevent->event.code;
	arg.event.flags=bevent->event.flags;
	arg.error.error=0;

	if (bevent->event.code & loop->BEVENT_ERR) (* bevent->cb[BEVENT_EVENT_INDEX_ERROR])(bevent, loop->BEVENT_ERR, &arg);
	if (bevent->event.code & loop->BEVENT_HUP) (* bevent->cb[BEVENT_EVENT_INDEX_CLOSE])(bevent, loop->BEVENT_HUP, &arg);
	if (bevent->event.code & loop->BEVENT_PRI) (* bevent->cb[BEVENT_EVENT_INDEX_PRI])(bevent, loop->BEVENT_PRI, &arg);
	if (bevent->event.code & loop->BEVENT_IN) (* bevent->cb[BEVENT_EVENT_INDEX_DATA])(bevent, loop->BEVENT_IN, &arg);
	if (bevent->event.code & loop->BEVENT_OUT) (* bevent->cb[BEVENT_EVENT_INDEX_WRITEABLE])(bevent, loop->BEVENT_OUT, &arg);
	goto processlist;

    }

}

unsigned char queue_bevent_events(struct beventloop_s *loop, uint32_t events, struct bevent_s *bevent)
{
    unsigned char count=0;
    uint32_t code=0;

    write_lock_list_element(&bevent->elist);
    code=(events & ~bevent->event.code); /* is this event not already reported here ? */

    // logoutput_debug("queue_bevent_events: fd %u events %u bevents %u code %u", (* bevent->ops->get_unix_fd)(bevent), events, bevent->event.code, code);

    if (code) {

        /* only queue it if not already */

        if (bevent->elist.h==NULL) {

	    write_lock_list_header(&loop->events);
            add_list_element_last(&loop->events, &bevent->elist);
            write_unlock_list_header(&loop->events);

        }

	bevent->event.code |= code; /* remember this event to prevent will be queued again and again and again ...  */
	count++;

    }

    write_unlock_list_element(&bevent->elist);
    return count;
}

struct bevent_s *get_next_bevent(struct beventloop_s *loop, struct bevent_s *bevent)
{
    struct list_element_s *list=NULL;

    if (bevent) {

	list=get_next_element(&bevent->list);

    } else {

	if (loop==NULL) loop=get_default_mainloop();
	list=get_list_head(&loop->bevents);

    }

    return ((list) ? (struct bevent_s *)((char *) list - offsetof(struct bevent_s, list)) : NULL);
}

static void set_bevent_cb_hlp(struct bevent_s *bevent, unsigned int flag, unsigned int event, void (* cb)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg))
{

    if (cb) {

        bevent->cb[event]=cb;
        bevent->flags |= flag;

    } else {

        bevent->cb[event]=cb_bevent_dummy;
        bevent->flags &= ~flag;

    }

}

void set_bevent_cb(struct bevent_s *bevent, unsigned int flag, void (* cb)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg))
{

    if (bevent) {

	if (flag & BEVENT_FLAG_CB_CLOSE) set_bevent_cb_hlp(bevent, BEVENT_FLAG_CB_CLOSE, BEVENT_EVENT_INDEX_CLOSE, cb);
	if (flag & BEVENT_FLAG_CB_ERROR) set_bevent_cb_hlp(bevent, BEVENT_FLAG_CB_ERROR, BEVENT_EVENT_INDEX_ERROR, cb);
	if (flag & BEVENT_FLAG_CB_DATA) set_bevent_cb_hlp(bevent, BEVENT_FLAG_CB_DATA, BEVENT_EVENT_INDEX_DATA, cb);
	if (flag & BEVENT_FLAG_CB_PRI) set_bevent_cb_hlp(bevent, BEVENT_FLAG_CB_PRI, BEVENT_EVENT_INDEX_PRI, cb);

    }

}

void set_bevent_ptr(struct bevent_s *bevent, void *ptr)
{
    if (bevent) bevent->ptr=ptr;
}

static uint32_t get_bevent_events_from_flags(struct bevent_s *bevent)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);
    uint32_t events=0;

    logoutput_debug("get_bevent_events_from_flags");

    if (loop) {

	if (bevent->flags & BEVENT_FLAG_CB_CLOSE) events |= loop->BEVENT_HUP;
	if (bevent->flags & BEVENT_FLAG_CB_ERROR) events |= loop->BEVENT_ERR;
	if (bevent->flags & BEVENT_FLAG_CB_DATA) events |= loop->BEVENT_IN;
	if (bevent->flags & BEVENT_FLAG_CB_WRITEABLE) events |= loop->BEVENT_OUT;
	if (bevent->flags & BEVENT_FLAG_CB_PRI) events |= loop->BEVENT_PRI;

    }

    return events;

}

void modify_bevent_watch(struct bevent_s *bevent)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);

    if (loop) {
	uint32_t events=get_bevent_events_from_flags(bevent);

	logoutput_debug("modify_bevent_watch: events %i", events);
	(* bevent->ops->ctl_io_bevent)(loop, bevent, BEVENT_CTL_MOD, events);

    }

}

int add_bevent_watch(struct bevent_s *bevent)
{
    int result=-1;
    struct beventloop_s *loop=NULL;

    logoutput_debug("add_bevent_watch");

    loop=get_eventloop_bevent(bevent);

    // logoutput_debug("add_bevent_watch: B");

    if (loop) {
	uint32_t events=get_bevent_events_from_flags(bevent); /* get events to set from the cb's set */

	logoutput_debug("add_bevent_watch: events %i", events);
	result=(* bevent->ops->ctl_io_bevent)(loop, bevent, BEVENT_CTL_ADD, events);

    }

    return result;
}

void remove_bevent_watch(struct bevent_s *bevent, unsigned int flags)
{

    if (bevent) {
	struct beventloop_s *loop=get_eventloop_bevent(bevent);

	if (loop) (* bevent->ops->ctl_io_bevent)(loop, bevent, BEVENT_CTL_DEL, 0);
	remove_io_event(bevent);

	if (flags & BEVENT_REMOVE_FLAG_UNSET) {

#ifdef __linux__

	    (* bevent->ops->set_unix_fd)(bevent, -1);

#endif

	}

    }

}

unsigned char bevent_set_property(struct bevent_s *bevent, const char *what, unsigned char enable)
{
    unsigned char set=(* bevent->ops->set_property)(bevent, what, enable);

    if (set) modify_bevent_watch(bevent);
    return set;

}

void free_bevent(struct bevent_s **p_bevent)
{
    struct bevent_s *bevent=*p_bevent;

    if (bevent) {
	struct beventloop_s *eloop=get_eventloop_bevent(bevent);

	remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);

	if (eloop) {
	    struct shared_signal_s *signal=eloop->signal;

	    signal_lock_flag(signal, &eloop->flags, BEVENTLOOP_FLAG_BEVENTS_LOCK);
	    remove_list_element(&bevent->list);
	    signal_unlock_flag(signal, &eloop->flags, BEVENTLOOP_FLAG_BEVENTS_LOCK);

	}

	(* bevent->ops->free_bevent)(&bevent);
	if (bevent) free(bevent);

    }

}

uint32_t signal_is_error(struct bevent_argument_s *arg)
{
    return ((arg->loop) ? (arg->event.code & arg->loop->BEVENT_ERR) : 0);
}

void disable_signal(struct bevent_argument_s *arg, unsigned int flag)
{
    if (flag & BEVENT_EVENT_FLAG_ERROR) (arg->event.code &= ~arg->loop->BEVENT_ERR);
    if (flag & BEVENT_EVENT_FLAG_WRITEABLE) (arg->event.code &= ~arg->loop->BEVENT_OUT);
    if (flag & BEVENT_EVENT_FLAG_CLOSE) (arg->event.code &= ~arg->loop->BEVENT_HUP);
    if (flag & BEVENT_EVENT_FLAG_DATA) (arg->event.code &= ~arg->loop->BEVENT_IN);
}

uint32_t signal_is_close(struct bevent_argument_s *arg)
{
    return ((arg->loop) ? (arg->event.code & arg->loop->BEVENT_HUP) : 0);
}

void disable_signal_error(struct bevent_argument_s *arg)
{
    if (arg->loop) (arg->event.code &= ~arg->loop->BEVENT_HUP);
}

uint32_t signal_is_data(struct bevent_argument_s *arg)
{
    return ((arg->loop) ? (arg->event.code & (arg->loop->BEVENT_IN | arg->loop->BEVENT_PRI)) : 0);
}

uint32_t signal_is_buffer(struct bevent_argument_s *arg)
{
    return ((arg->loop) ? (arg->event.code & arg->loop->BEVENT_OUT) : 0);
}

unsigned int printf_event_uint(struct bevent_argument_s *arg)
{
    return (unsigned int) (arg->event.code);
}

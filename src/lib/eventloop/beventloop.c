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

static struct beventloop_s beventloop_main;
static pthread_mutex_t global_mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned char init=0;
static char *name="beventloop";

int lock_beventloop(struct beventloop_s *loop)
{
    return pthread_mutex_lock(&global_mutex);
}

int unlock_beventloop(struct beventloop_s *beventloop)
{
    return pthread_mutex_unlock(&global_mutex);
}

static int add_bevent_beventloop(struct beventloop_s *loop, struct bevent_s *bevent, uint32_t code)
{
    int result=-1;

    if (bevent==NULL || bevent->loop || bevent->fd<0 || (bevent->flags & BEVENT_FLAG_EVENTLOOP)) return -1;
    if (loop==NULL) loop=&beventloop_main;

    if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {
	struct epoll_event e_event;

	if (loop->type.epoll_fd<0) {

	    logoutput_warning("add_bevent_beventloop: epoll fd not set");
	    goto failed;

	}

	e_event.events=map_bevent_to_epollevent(code);
	if (e_event.events==0) {

	    logoutput_warning("add_bevent_beventloop: events zero");
	    goto failed;

	}

	e_event.data.ptr=(void *) bevent;

	result=epoll_ctl(loop->type.epoll_fd, EPOLL_CTL_ADD, bevent->fd, &e_event);
	if (result==0) {

	    bevent->flags |= BEVENT_FLAG_EVENTLOOP;
	    bevent->loop=loop;
	    bevent->code=code;

	}

    }

    return result;

    failed:

    logoutput_warning("add_bevent_beventloop: failed to add event to eventloop");
    return -1;
}

static void remove_bevent_beventloop(struct bevent_s *bevent)
{
    struct beventloop_s *loop=NULL;

    if (bevent==NULL || bevent->loop==NULL || bevent->fd<0 || (bevent->flags & BEVENT_FLAG_EVENTLOOP)==0) return;
    loop=bevent->loop;

    if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {
	struct epoll_event e_event;

	if (loop->type.epoll_fd<0) return;

	int result=epoll_ctl(loop->type.epoll_fd, EPOLL_CTL_DEL, bevent->fd, &e_event);
	if (result==0) {

	    bevent->flags -= BEVENT_FLAG_EVENTLOOP;
	    bevent->loop=NULL;
	    bevent->code=0;

	}

    }

    return;
}

static void modify_bevent_beventloop(struct bevent_s *bevent, uint32_t event)
{
    struct beventloop_s *loop=NULL;

    if (bevent==NULL || bevent->loop==NULL || bevent->fd<0 || (bevent->flags & BEVENT_FLAG_EVENTLOOP)==0 || bevent->code == event) return;
    loop=bevent->loop;

    if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {
	struct epoll_event e_event;

	if (loop->type.epoll_fd<0) return;
	e_event.events=map_bevent_to_epollevent(event);
	e_event.data.ptr=(void *) bevent;

	int result=epoll_ctl(loop->type.epoll_fd, EPOLL_CTL_MOD, bevent->fd, &e_event);
	if (result==0) {

	    bevent->code=event;

	}

    }

    return;
}

static void signal_cb_dummy(struct beventloop_s *b, void *data, struct signalfd_siginfo *fdsi)
{
    logoutput("signal_cb_dummy");
}

static void _run_expired_dummy(struct beventloop_s *loop)
{
}

static void _init_beventloop(struct beventloop_s *loop)
{
    struct timerlist_s *timers=&loop->timers;

    memset(loop, 0, sizeof(struct beventloop_s));
    loop->status=0;
    loop->flags|=BEVENTLOOP_FLAG_EPOLL; /* default  */
    init_list_header(&loop->bevents, SIMPLE_LIST_TYPE_EMPTY, NULL);
    loop->bevents.name="bevents";

    loop->cb_signal=signal_cb_dummy;
    loop->type.epoll_fd=-1;
    loop->add_bevent=add_bevent_beventloop;
    loop->remove_bevent=remove_bevent_beventloop;
    loop->modify_bevent=modify_bevent_beventloop;

    init_list_header(&timers->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    loop->timers.header.name="timers";
    timers->fd=-1;
    timers->run_expired=_run_expired_dummy;
    pthread_mutex_init(&timers->mutex, NULL);
    timers->threadid=0;
}

struct beventloop_s *create_beventloop()
{
    struct beventloop_s *loop=malloc(sizeof(struct beventloop_s));

    if (loop) {

	memset(loop, 0, sizeof(struct beventloop_s));
	loop->flags=BEVENTLOOP_FLAG_ALLOC;

    }

    return loop;
}

int init_beventloop(struct beventloop_s *loop)
{

    if (! loop) loop=&beventloop_main;

    pthread_mutex_lock(&global_mutex);

    if (loop==&beventloop_main) {

	if (init==0) {

	    logoutput("init_beventloop: init mainloop");
	    _init_beventloop(loop);
	    loop->flags |= BEVENTLOOP_FLAG_MAIN;
	    init=1;

	} else {

	    pthread_mutex_unlock(&global_mutex);
	    return 0;

	}

    } else {

	_init_beventloop(loop);

    }

    pthread_mutex_unlock(&global_mutex);

    if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {

	/* create an epoll instance */

	loop->type.epoll_fd=epoll_create(MAX_EPOLL_NRFDS);

	if (loop->type.epoll_fd==-1) {

	    logoutput_warning("init_beventloop: error %i creating epoll instance (%s)", errno, strerror(errno));
	    goto error;

	}

    } else {

	logoutput_warning("init_beventloop: type beventloop not supported");
	goto error;

    }

    return 0;

    error:

    if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {

	if (loop->type.epoll_fd>0) {

	    close(loop->type.epoll_fd);
	    loop->type.epoll_fd=-1;

	}

	loop->status=BEVENTLOOP_STATUS_DOWN;

    }

    return -1;

}

uint32_t map_epollevent_to_bevent(uint32_t e_event)
{
    uint32_t code=0;
    code |= ((e_event & EPOLLIN) ? BEVENT_CODE_IN : 0);
    code |= ((e_event & EPOLLOUT) ? BEVENT_CODE_OUT : 0);
    code |= ((e_event & EPOLLHUP) ? BEVENT_CODE_HUP : 0);
    code |= ((e_event & EPOLLERR) ? BEVENT_CODE_ERR : 0);
    code |= ((e_event & EPOLLPRI) ? BEVENT_CODE_PRI : 0);
    return code;
};

uint32_t map_bevent_to_epollevent(uint32_t code)
{
    uint32_t e_event=0;
    e_event |= ((code & BEVENT_CODE_IN) ? EPOLLIN : 0);
    e_event |= ((code & BEVENT_CODE_OUT) ? EPOLLOUT : 0);
    e_event |= ((code & BEVENT_CODE_HUP) ? EPOLLHUP : 0);
    e_event |= ((code & BEVENT_CODE_ERR) ? EPOLLERR : 0);
    e_event |= ((code & BEVENT_CODE_PRI) ? EPOLLPRI : 0);
    return e_event;
}

int start_beventloop(struct beventloop_s *loop)
{
    struct bevent_s *bevent=NULL;
    int result=0;

    if (! loop) loop=&beventloop_main;

    if (loop->status==BEVENTLOOP_STATUS_UP) return 0;
    loop->status=BEVENTLOOP_STATUS_UP;

    while (loop->status==BEVENTLOOP_STATUS_UP) {

	if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {
	    struct epoll_event epoll_events[MAX_EPOLL_NREVENTS];

    	    int count=epoll_wait(loop->type.epoll_fd, epoll_events, MAX_EPOLL_NREVENTS, -1);

    	    if (count<0) {

		loop->status=BEVENTLOOP_STATUS_DOWN;

	    } else {

    		for (unsigned int i=0; i<count; i++) {

        	    bevent=(struct bevent_s *) epoll_events[i].data.ptr;
		    result=(* bevent->cb) (bevent->fd, bevent->data, map_epollevent_to_bevent(epoll_events[i].events));

		}

	    }

        }

    }

    loop->status=BEVENTLOOP_STATUS_DOWN;

    if (loop->flags & BEVENTLOOP_FLAG_EPOLL) {

	if (loop->type.epoll_fd>=0) {

	    close(loop->type.epoll_fd);
	    loop->type.epoll_fd=-1;

	}

    }

    if (loop->timers.fd>=0) {

	close(loop->timers.fd);
	loop->timers.fd=-1;

    }

    out:

    return 0;
}

void stop_beventloop(struct beventloop_s *loop)
{
    if (!loop) loop=&beventloop_main;
    loop->status=BEVENTLOOP_STATUS_DOWN;
}

void clear_beventloop(struct beventloop_s *loop)
{
    struct list_element_s *list=NULL;

    if (! loop) loop=&beventloop_main;
    lock_beventloop(loop);

    getbevent:

    list=get_list_head(&loop->bevents, SIMPLE_LIST_FLAG_REMOVE);

    if (list) {
	struct bevent_s *bevent=(struct bevent_s *)(((char *)list) - offsetof(struct bevent_s, list));

	/* already removed from list */
	if (bevent->flags & BEVENT_FLAG_LIST) bevent->flags -= BEVENT_FLAG_LIST;
	if (bevent->flags & BEVENT_FLAG_EVENTLOOP) (* loop->remove_bevent)(bevent);
	if (bevent->flags & BEVENT_FLAG_ALLOCATED) free(bevent);
	bevent=NULL;

	goto getbevent;

    }

    if (loop->type.epoll_fd>0) {

	close(loop->type.epoll_fd);
	loop->type.epoll_fd=0;

    }

    /* free any timer still in queue */

    pthread_mutex_lock(&loop->timers.mutex);

    gettimer:

    list=get_list_head(&loop->timers.header, SIMPLE_LIST_FLAG_REMOVE);

    if (list) {
	struct timerentry_s *entry=(struct timerentry_s *)(((char *)list) - offsetof(struct timerentry_s, list));

	free(entry);
	goto gettimer;

    }

    pthread_mutex_unlock(&loop->timers.mutex);
    pthread_mutex_destroy(&loop->timers.mutex);
    unlock_beventloop(loop);

}

void finish_beventloop(struct beventloop_s **p_loop)
{
    struct beventloop_s *loop=*p_loop;
    clear_beventloop(loop);
    pthread_mutex_destroy(&loop->timers.mutex);
    if (loop->flags & BEVENTLOOP_FLAG_ALLOC) {

	free(loop);
	*p_loop=NULL;

    }

}

struct beventloop_s *get_mainloop()
{
    return &beventloop_main;
}

uint32_t signal_is_error(uint32_t event)
{
    return (event & BEVENT_CODE_ERR);
}

uint32_t signal_is_hangup(uint32_t event)
{
    return (event & (BEVENT_CODE_RDHUP | BEVENT_CODE_HUP));
}

uint32_t signal_is_dataavail(uint32_t event)
{
    return (event & BEVENT_CODE_IN);
}
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

#include <sys/wait.h>
#include <sys/epoll.h>

#include "libosns-misc.h"
#include "libosns-log.h"
#include "libosns-eventloop.h"

/* there are two kinds of flags:
    - signals like EPOLLIN and EPOLLHUP : used as input for epoll_ctl AND as event in epoll_event->events
    - properties like EPOLLET and EPOLLONESHOT : (only) used as input for epoll_ctl
*/

#define EPOLL_FLAG_ALL				(EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP)
#define EPOLL_PROP_ALL				(EPOLLET | EPOLLONESHOT | EPOLLWAKEUP | EPOLLEXCLUSIVE)

struct _bevent_epoll_s {
    uint32_t			events;
    uint32_t			properties;
    int				fd;
    struct bevent_s		common;
};

static struct bevent_ops_s epoll_bevent_ops;
static struct beventloop_ops_s epoll_loop_ops;

static int ctl_io_bevent_epoll(struct beventloop_s *loop, struct bevent_s *bevent, unsigned char op, unsigned int events)
{
    struct _bevent_epoll_s *tmp=(struct _bevent_epoll_s *)((char *) bevent - offsetof(struct _bevent_epoll_s, common));
    int result=-1;

    events &= EPOLL_FLAG_ALL;
    if (events==0 && op==BEVENT_CTL_MOD) return -1;

    if (op==BEVENT_CTL_DEL) {

	if (bevent->flags & BEVENT_FLAG_ENABLED) {

	    if (epoll_ctl(loop->backend.epoll.fd, EPOLL_CTL_DEL, tmp->fd, NULL)==0) {

		bevent->flags &= ~BEVENT_FLAG_ENABLED;
		tmp->events=0;
		tmp->properties=0;
		result=0;

	    } else {
		unsigned int error=errno;

		logoutput_warning("ctl_bevent_epoll: error %i removing fd %i from eventloop (%s)", error, tmp->fd, strerror(error));

	    }

	}

    } else if (op==BEVENT_CTL_MOD) {

	if (bevent->flags & BEVENT_FLAG_ENABLED) {
	    struct epoll_event epev;

	    epev.events=(events | tmp->properties | loop->BEVENT_INIT);
	    epev.data.ptr=(void *) bevent;
 
	    if (epoll_ctl(loop->backend.epoll.fd, EPOLL_CTL_MOD, tmp->fd, &epev)==0) {

		tmp->events=events;
		result=0;

	    } else {
		unsigned int error=errno;

		logoutput_warning("ctl_bevent_epoll: error %i modifying fd %i events %i (%s)", error, tmp->fd, events, strerror(error));

	    }

	} else {

	    /* cold modify */

	    result=0;
	    tmp->events=events;

	}

    } else if (op==BEVENT_CTL_ADD) {

	if ((bevent->flags & BEVENT_FLAG_ENABLED)==0) {
	    struct epoll_event epev;

	    if (events==0) events=tmp->events;

	    epev.events=(events | tmp->properties | loop->BEVENT_INIT);
	    epev.data.ptr=(void *) bevent;

	    if (epoll_ctl(loop->backend.epoll.fd, EPOLL_CTL_ADD, tmp->fd, &epev)==0) {

		tmp->events=events;
		bevent->flags |= BEVENT_FLAG_ENABLED;
		result=0;

		logoutput("ctl_bevent_epoll: added fd %i events %i", tmp->fd, events);

	    } else {
		unsigned int error=errno;
		logoutput_warning("ctl_bevent_epoll: error %i adding fd %i events %i (%s)", error, tmp->fd, events, strerror(error));

	    }

	} else {

	    logoutput_warning("ctl_bevent_epoll: fd %i already added", tmp->fd);

	}

    }

    return result;

}

static unsigned char set_property_bevent_epoll(struct _bevent_epoll_s *tmp, unsigned int flag, unsigned char enable)
{
    unsigned char set=0;

    if (enable) {

	tmp->properties |= flag;
	set=1;

    } else {

	if (tmp->properties & flag) {

	    tmp->properties &= ~flag;
	    set=1;

	}

    }

    return set;
}

static unsigned char set_property_epoll(struct bevent_s *bevent, const char *what, unsigned char enable)
{
    struct _bevent_epoll_s *tmp=(struct _bevent_epoll_s *)((char *) bevent - offsetof(struct _bevent_epoll_s, common));
    unsigned char set=0;

    if (strcmp(what, "edge-triggered")==0) {

	set=set_property_bevent_epoll(tmp, EPOLLET, enable);

    } else if (strcmp(what, "oneshot")==0) {

	set=set_property_bevent_epoll(tmp, EPOLLONESHOT, enable);

    }

    return set;

}

static void free_bevent_epoll(struct bevent_s **p_bevent)
{
    struct bevent_s *bevent=*p_bevent;

    if (bevent) {
	struct _bevent_epoll_s *tmp=(struct _bevent_epoll_s *)((char *) bevent - offsetof(struct _bevent_epoll_s, common));

	free(tmp);
	*p_bevent=NULL;

    }

}

static void set_bevent_unix_fd_epoll(struct bevent_s *bevent, int fd)
{
    struct _bevent_epoll_s *tmp=(struct _bevent_epoll_s *)((char *) bevent - offsetof(struct _bevent_epoll_s, common));
    tmp->fd = fd;
}

static int get_bevent_unix_fd_epoll(struct bevent_s *bevent)
{
    struct _bevent_epoll_s *tmp=(struct _bevent_epoll_s *)((char *) bevent - offsetof(struct _bevent_epoll_s, common));
    return (int) tmp->fd;
}

static struct bevent_ops_s epoll_bevent_ops = {
    .ctl_io_bevent			= ctl_io_bevent_epoll,
    .set_property			= set_property_epoll,
    .free_bevent			= free_bevent_epoll,
    .get_unix_fd			= get_bevent_unix_fd_epoll,
    .set_unix_fd			= set_bevent_unix_fd_epoll,
};

static void start_beventloop_epoll(struct beventloop_s *loop)
{
    struct shared_signal_s *signal=NULL;
    struct _beventloop_epoll_s *tmp=NULL;

    if (loop==NULL) return;
    tmp=&loop->backend.epoll;
    signal=loop->signal;

    while (tmp->fd>=0) {
	struct epoll_event aevents[45]; /* arbritary */

	(* loop->first_run)(loop);

	int result=epoll_wait(tmp->fd, aevents, 45, -1);

	logoutput_debug("start_beventloop_epoll: wake up: %i", result);

	if (result>0) {

	    for (unsigned int i=0; i<result; i++) {
		struct bevent_s *bevent=(struct bevent_s *) aevents[i].data.ptr;
		uint32_t events=aevents[i].events;
		struct bevent_argument_s arg;
		int fd=((bevent) ? ((* bevent->ops->get_unix_fd)(bevent)) : -1);

		logoutput_debug("start_beventloop_epoll: i %u events %u fd %i", i, events, fd);

		arg.loop=loop;
		arg.event.code=events;
		arg.event.flags=((events & EPOLLPRI) ? BEVENT_EVENT_FLAG_PRI : 0);

		if (arg.event.code & EPOLLERR) {

		    (* bevent->cb_error)(bevent, BEVENT_EVENT_FLAG_ERROR, &arg);

		}

		if (arg.event.code & (EPOLLHUP | EPOLLRDHUP)) {

		    (* bevent->cb_close)(bevent, BEVENT_EVENT_FLAG_CLOSE, &arg);

		}

		if (arg.event.code & EPOLLPRI) {

		    (* bevent->cb_pri)(bevent, BEVENT_EVENT_FLAG_PRI, &arg);

		}

		if (arg.event.code & EPOLLIN) {

		    (* bevent->cb_dataavail)(bevent, BEVENT_EVENT_FLAG_DATAAVAIL, &arg);

		}

		if (arg.event.code & EPOLLOUT) {

		    (* bevent->cb_writeable)(bevent, BEVENT_EVENT_FLAG_WRITEABLE, &arg);

		}

		aevents[i].events=0;

	    }

	} else if (result==-1) {

	    if (errno==EBADF || tmp->fd==-1) {

		logoutput_warning("start_beventloop_epoll: epoll fd closed");
		break;

	    }

	    logoutput_warning("start_beventloop_epoll: error %i epoll_wait (%s)", errno, strerror(errno));

	}

    }

}

static void stop_beventloop_epoll(struct beventloop_s *loop)
{
    struct _beventloop_epoll_s *tmp=NULL;
    struct shared_signal_s *signal=NULL;

    if (loop==NULL) return;
    tmp=&loop->backend.epoll;
    signal=loop->signal;

    if (tmp->fd>=0) {

	close(tmp->fd);
	tmp->fd=-1;

    }

    signal_set_flag(signal, &loop->flags, BEVENTLOOP_FLAG_STOP);

}

static void clear_beventloop_epoll(struct beventloop_s *loop)
{
    struct shared_signal_s *signal=loop->signal;
    signal_set_flag(signal, &loop->flags, BEVENTLOOP_FLAG_CLEAR);
}

static int init_io_bevent_epoll(struct beventloop_s *eloop, struct bevent_s *bevent)
{
    struct _bevent_epoll_s *tmp=(struct _bevent_epoll_s *)((char *) bevent - offsetof(struct _bevent_epoll_s, common));

    tmp->events 	= 0;
    tmp->properties	= 0;
    tmp->fd		=-1;
    bevent->ops		= &epoll_bevent_ops;
    return 0;
}

static struct bevent_s *create_io_bevent_epoll(struct beventloop_s *eloop)
{
    struct _bevent_epoll_s *tmp=malloc(sizeof(struct _bevent_epoll_s));
    struct bevent_s *bevent=NULL;

    if (tmp) {

	memset(tmp, 0, sizeof(struct _bevent_epoll_s));
	tmp->fd=-1;
	bevent=&tmp->common;
	init_bevent(bevent);
	bevent->flags=BEVENT_FLAG_CREATE;
	bevent->ops= &epoll_bevent_ops;

    }

    return bevent;
}

static struct beventloop_ops_s epoll_loop_ops = {
    .start_eventloop			= start_beventloop_epoll,
    .stop_eventloop			= stop_beventloop_epoll,
    .clear_eventloop			= clear_beventloop_epoll,
    .create_bevent			= create_io_bevent_epoll,
    .init_io_bevent			= init_io_bevent_epoll,
};

void set_beventloop_epoll(struct beventloop_s *loop)
{
    if (loop==NULL) return;
    loop->ops=&epoll_loop_ops;
    loop->BEVENT_IN=EPOLLIN;
    loop->BEVENT_OUT=EPOLLOUT;
    loop->BEVENT_ERR=EPOLLERR;
    loop->BEVENT_HUP=EPOLLHUP | EPOLLRDHUP;
    loop->BEVENT_PRI=EPOLLPRI;
    loop->BEVENT_INIT=(loop->BEVENT_ERR | loop->BEVENT_HUP);
}

int init_beventloop_epoll(struct beventloop_s *loop)
{
    struct _beventloop_epoll_s *tmp=NULL;

    logoutput("init_beventloop_epoll");

    if (loop==NULL) return -1;

    tmp=&loop->backend.epoll;
    tmp->flags=0;
    tmp->fd=epoll_create1(0);

    if (tmp->fd==-1) {
	unsigned int error=errno;

	logoutput_warning("init_beventloop_epoll: error %i creating epoll instance (%s)", error, strerror(error));
	return -1;

    }

    set_beventloop_epoll(loop);
    return tmp->fd;

}

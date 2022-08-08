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

#ifndef _LIB_EVENTLOOP_BACKEND_SIGNALFD_H
#define _LIB_EVENTLOOP_BACKEND_SIGNALFD_H

struct bsignal_event_s {
    uint32_t				signo;
    struct beventloop_s 		*loop;
    union _bsignal_signo_u {
	struct _bsignal_io {
	    uint32_t			pid;
	    uint32_t			fd;
	    uint32_t			events;
	} io;
	struct _bsignal_chld {
	    uint32_t			pid;
	    uint32_t			uid;
	    int				status;
	    uint64_t			utime;
	    uint64_t			stime;
	} chld;
	struct _bsignal_kill {
	    uint32_t			uid;
	    uint32_t			pid;
	} kill;
    } type;
};

/* Prototypes */

int init_signalfd_subsystem(struct beventloop_s *loop, struct bevent_subsystem_s *subsys);
void set_cb_signalfd_subsystem(struct bevent_subsystem_s *subsys, void (* cb)(struct beventloop_s *loop, struct bsignal_event_s *bse));

#endif

/*
  2010, 2011, 2012, 2013 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_EVENTLOOP_BEVENTLOOP_H
#define _LIB_EVENTLOOP_BEVENTLOOP_H

#include <glib.h>
#include "list.h"

struct event_s {
    struct beventloop_s 			*loop;
    union _event_type_u {
	gushort					glib_revents;
    } events;
};

/* struct to identify the fd when eventloop signals activity on that fd */

#define BEVENT_FLAG_FD				1
#define BEVENT_FLAG_TIMEOUT			2
#define BEVENT_FLAG_EVENTLOOP			4

struct bevent_s {
    union _loop_type_s {
	struct _bevent_glib_s {
	    GSource				source;
	    GPollFD				pollfd;
	} glib;
    } ltype;
    unsigned int				flags;
    struct beventloop_s 			*loop;
    union _bevent_type_s {
	struct _bevent_fd_s {
	    void 				(* cb)(int fd, void *ptr, struct event_s *event);
	} fd;
	struct _bevent_timeout_s {
	    void				(* cb)(unsigned int id, void *ptr);
	} timeout;
    } btype;
    void					*ptr;
    void					(* close)(struct bevent_s *b);
};

/* eventloop */

#define BEVENTLOOP_FLAG_ALLOC			1 << 0
#define BEVENTLOOP_FLAG_MAIN			1 << 1
#define BEVENTLOOP_FLAG_INIT			1 << 2
#define BEVENTLOOP_FLAG_SIGNAL			1 << 3
#define BEVENTLOOP_FLAG_RUNNING			1 << 4

#define BEVENTLOOP_FLAG_GLIB			1 << 12

struct beventloop_s {
    unsigned int				flags;
    union _loop_used_s {
	struct _loop_glib_s {
	    GMainLoop				*loop;
	    GSourceFuncs			funcs;
	} glib;
    } type;
    pthread_mutex_t				mutex;
    pthread_cond_t				cond;
    struct list_header_s			timers;
    uint32_t (* event_is_error)(struct event_s *event);
    uint32_t (* event_is_close)(struct event_s *event);
    uint32_t (* event_is_data)(struct event_s *event);
    uint32_t (* event_is_buffer)(struct event_s *event);
};

/* Prototypes */

struct bevent_s *create_fd_bevent(struct beventloop_s *eloop, void (* cb)(int fd, void *ptr, struct event_s *event), void *ptr);
int add_bevent_beventloop(struct bevent_s *bevent);

void set_bevent_unix_fd(struct bevent_s *bevent, int fd);
int get_bevent_unix_fd(struct bevent_s *bevent);

void set_bevent_watch(struct bevent_s *bevent, const char *what);
void remove_bevent(struct bevent_s *bevent);

unsigned int create_timer_eventloop(struct beventloop_s *eloop, struct timespec *timeout, void (* cb)(unsigned int id, void *ptr), void *ptr);

int init_beventloop(struct beventloop_s *b);
int start_beventloop(struct beventloop_s *b);
void stop_beventloop(struct beventloop_s *b);
void clear_beventloop(struct beventloop_s *b);
void free_beventloop(struct beventloop_s **p_b);

struct beventloop_s *get_mainloop();

uint32_t signal_is_error(struct event_s *event);
uint32_t signal_is_close(struct event_s *event);
uint32_t signal_is_data(struct event_s *event);
uint32_t signal_is_buffer(struct event_s *event);

unsigned int printf_event_uint(struct event_s *event);

#endif

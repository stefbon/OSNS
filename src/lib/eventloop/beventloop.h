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

#define BEVENTLOOP_MAX_SUBSYSTEMS		12

#include "libosns-list.h"
#include "system/time.h"

struct beventloop_s;
struct bevent_s;

#ifdef __linux__
#include "backend/linux-epoll.h"
#endif
#include "backend/glib-mainloop.h"


#define BEVENT_EVENT_FLAG_ERROR			1
#define BEVENT_EVENT_FLAG_CLOSE			2
#define BEVENT_EVENT_FLAG_DATAAVAIL		4
#define BEVENT_EVENT_FLAG_WRITEABLE		8
#define BEVENT_EVENT_FLAG_PRI			16

/* struct to identify the fd when eventloop signals activity on that fd */

#define BEVENT_FLAG_CREATE			(1 << 0)
#define BEVENT_FLAG_FD				(1 << 1)
#define BEVENT_FLAG_TIMEOUT			(1 << 2)
#define BEVENT_FLAG_EVENTLOOP			(1 << 3)
#define BEVENT_FLAG_ENABLED			(1 << 4)

#define BEVENT_FLAG_WRITE			(1 << 8)
#define BEVENT_FLAG_BLOCKED			(1 << 9)
#define BEVENT_FLAG_WRITABLE			(1 << 10)

#define BEVENT_FLAG_CTL				(1 << 11)

#define BEVENT_FLAG_CB_ERROR			(BEVENT_EVENT_FLAG_ERROR << 12)
#define BEVENT_FLAG_CB_CLOSE			(BEVENT_EVENT_FLAG_CLOSE << 12)
#define BEVENT_FLAG_CB_DATAAVAIL		(BEVENT_EVENT_FLAG_DATAAVAIL << 12)
#define BEVENT_FLAG_CB_WRITEABLE		(BEVENT_EVENT_FLAG_WRITEABLE << 12)
#define BEVENT_FLAG_CB_PRI			(BEVENT_EVENT_FLAG_PRI << 12)

#define BEVENT_REMOVE_FLAG_UNSET		1

struct bevent_ops_s {
    int						(* ctl_io_bevent)(struct beventloop_s *loop, struct bevent_s *b, unsigned char op, unsigned int events);
    unsigned char				(* set_property)(struct bevent_s *b, const char *what, unsigned char enable);
    void					(* free_bevent)(struct bevent_s **b);
    int						(* get_unix_fd)(struct bevent_s *b);
    void					(* set_unix_fd)(struct bevent_s *b, int fd);
};

struct bevent_error_s {
#ifdef __linux__
    unsigned int				error;
#endif
};

struct bevent_event_s {
    unsigned int				code;
    unsigned int				flags;
};

struct bevent_argument_s {
    struct beventloop_s				*loop;
    struct bevent_event_s			event;
    struct bevent_error_s			error;
};

struct bevent_s {
    unsigned int				flags;
    struct list_element_s			list;
    struct system_timespec_s			unblocked;
    struct bevent_ops_s				*ops;
    void 					(* cb_dataavail)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg);
    void 					(* cb_writeable)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg);
    void 					(* cb_close)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg);
    void 					(* cb_error)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg);
    void 					(* cb_pri)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg);
    struct osns_socket_s			*sock;
    void					*ptr;
};

#define BEVENT_CTL_ADD				1
#define BEVENT_CTL_MOD				2
#define BEVENT_CTL_DEL				3

struct beventloop_ops_s {
    void					(* start_eventloop)(struct beventloop_s *loop);
    void					(* stop_eventloop)(struct beventloop_s *loop);
    void					(* clear_eventloop)(struct beventloop_s *loop);
    struct bevent_s				*(* create_bevent)(struct beventloop_s *loop);
    int						(* init_io_bevent)(struct beventloop_s *loop, struct bevent_s *bevent);
};

/* subsystems like:
    - system signals (SIG_CHILD, SIG_IO)
    - timers
    - fsnotify
*/

struct bevent_subsystem_s;

struct bevent_subsystem_ops_s {
    int						(* start_subsys)(struct bevent_subsystem_s *subsys);
    void					(* stop_subsys)(struct bevent_subsystem_s *subsys);
    void					(* clear_subsys)(struct bevent_subsystem_s *subsys);
};

#define BEVENT_SUBSYSTEM_FLAG_ALLOC		1
#define BEVENT_SUBSYSTEM_FLAG_START		2
#define BEVENT_SUBSYSTEM_FLAG_STOP		4
#define BEVENT_SUBSYSTEM_FLAG_CLEAR		8

#define BEVENT_SUBSYSTEM_FLAG_DUMMY		32

struct bevent_subsystem_s {
    unsigned int				flags;
    const char					*type_name;
    const char					*name;
    struct bevent_subsystem_ops_s		*ops;
    unsigned int				size;
    char					buffer[];
};

/* eventloop */

#define BEVENTLOOP_FLAG_ALLOC			1 << 0
#define BEVENTLOOP_FLAG_MAIN			1 << 1
#define BEVENTLOOP_FLAG_INIT			1 << 2
#define BEVENTLOOP_FLAG_SUBSYSTEMS_LOCK		1 << 3
#define BEVENTLOOP_FLAG_BEVENTS_LOCK		1 << 4

#define BEVENTLOOP_FLAG_START			1 << 5
#define BEVENTLOOP_FLAG_STOP			1 << 6
#define BEVENTLOOP_FLAG_CLEAR			1 << 7

#define BEVENTLOOP_FLAG_GLIB			1 << 12
#define BEVENTLOOP_FLAG_EPOLL			1 << 13

struct beventloop_s {
    unsigned int				flags;
    struct beventloop_ops_s			*ops;
    struct shared_signal_s			*signal;
    void					*ptr;
    void					(* first_run)(struct beventloop_s *loop);
    void					(* first_run_ctx_cb)(struct beventloop_s *loop, void *ptr);
    struct list_header_s			bevents;
    struct bevent_subsystem_s			*asubsystems[BEVENTLOOP_MAX_SUBSYSTEMS];
    unsigned int				count;
    uint32_t					BEVENT_IN;
    uint32_t					BEVENT_OUT;
    uint32_t					BEVENT_ERR;
    uint32_t					BEVENT_HUP;
    uint32_t					BEVENT_PRI;
    uint32_t					BEVENT_INIT;
    uint32_t					BEVENT_ET;
    union {
#ifdef __linux__
	struct _beventloop_epoll_s 		epoll;
#endif
	struct _beventloop_glib_s		glib;
    } backend;
};

/* Prototypes */

int init_beventloop(struct beventloop_s *b);

void set_first_run_beventloop(struct beventloop_s *eloop, void (* cb)(struct beventloop_s *eloop, void *ptr), void *ptr);

void start_beventloop(struct beventloop_s *b);
void stop_beventloop(struct beventloop_s *b);
void clear_beventloop(struct beventloop_s *b);
void free_beventloop(struct beventloop_s **p_b);

struct beventloop_s *get_default_mainloop();
void set_type_beventloop(struct beventloop_s *loop, unsigned int flag);

#endif

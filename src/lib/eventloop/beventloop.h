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

#ifndef _COMMON_UTILS_BEVENTLOOP_H
#define _COMMON_UTILS_BEVENTLOOP_H

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

#include "list.h"

#define MAX_EPOLL_NREVENTS 			32
#define MAX_EPOLL_NRFDS				32

#define BEVENTLOOP_OK				0
#define BEVENTLOOP_EXIT				-1

#define BEVENTLOOP_STATUS_NOTSET		0
#define BEVENTLOOP_STATUS_SETUP			1
#define BEVENTLOOP_STATUS_UP			2
#define BEVENTLOOP_STATUS_DOWN			3

#define BEVENTLOOP_FLAG_TIMER			1
#define BEVENTLOOP_FLAG_SIGNAL			2
#define BEVENTLOOP_FLAG_MAIN			4
#define BEVENTLOOP_FLAG_EPOLL			8
#define BEVENTLOOP_FLAG_ALLOC			16

#define TIMERENTRY_STATUS_NOTSET		0
#define TIMERENTRY_STATUS_ACTIVE		1
#define TIMERENTRY_STATUS_INACTIVE		2
#define TIMERENTRY_STATUS_BUSY			3

#define BEVENT_FLAG_ALLOCATED			1
#define BEVENT_FLAG_EVENTLOOP			2
#define BEVENT_FLAG_LIST			4
#define BEVENT_FLAG_TIMER			8
#define BEVENT_FLAG_SIGNAL			16

#define BEVENT_NAME_LEN				32

#define TIMERID_TYPE_PTR			1
#define TIMERID_TYPE_UNIQUE			2

#define BEVENT_CODE_IN				EPOLLIN
#define BEVENT_CODE_OUT				EPOLLOUT
#define BEVENT_CODE_ERR				EPOLLERR
#define BEVENT_CODE_HUP				EPOLLHUP
#define BEVENT_CODE_PRI				EPOLLPRI

typedef int (*bevent_cb)(int fd, void *data, uint32_t eventcode);

struct timerid_s {
    void					*context;
    union {
	void					*ptr;
	uint64_t				unique;
    } id;
    unsigned char 				type;
};

struct timerentry_s {
    struct timespec 				expire;
    unsigned char 				status;
    unsigned long 				ctr;
    void 					(*eventcall) (struct timerid_s *id, struct timespec *now);
    struct timerid_s				id;
    struct beventloop_s 			*loop;
    struct list_element_s			list;
};

struct beventloop_s;

struct timerlist_s {
    struct list_header_s			header;
    pthread_mutex_t 				mutex;
    pthread_t					threadid;
    int						fd;
    void					(* run_expired)(struct beventloop_s *loop);
};

/* struct to identify the fd when eventloop signals activity on that fd */

struct bevent_s {
    int 					fd;
    void 					*data;
    unsigned char 				flags;
    uint32_t					code;
    bevent_cb 					cb;
    char 					name[BEVENT_NAME_LEN];
    struct list_element_s			list;
    struct beventloop_s 			*loop;
};

/* eventloop */

struct beventloop_s {
    unsigned char 				status;
    unsigned int				flags;
    struct list_header_s			bevents;
    struct timerlist_s				timers;
    void 					(* cb_signal) (struct beventloop_s *loop, void *data, struct signalfd_siginfo *fdsi);
    int						(* add_bevent)(struct beventloop_s *loop, struct bevent_s *bevent, uint32_t eventcode);
    void					(* remove_bevent)(struct bevent_s *bevent);
    void					(* modify_bevent)(struct bevent_s *bevent, uint32_t event);
    union {
	int 					epoll_fd;
    } type;
};

/* Prototypes */

struct beventloop_s *create_beventloop();
int init_beventloop(struct beventloop_s *b);
int start_beventloop(struct beventloop_s *b);
void stop_beventloop(struct beventloop_s *b);
void clear_beventloop(struct beventloop_s *b);

struct beventloop_s *get_mainloop();

uint32_t map_epollevent_to_bevent(uint32_t e_event);
uint32_t map_bevent_to_epollevent(uint32_t code);

#endif

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

#ifndef _LIB_EVENTLOOP_BEVENT_H
#define _LIB_EVENTLOOP_BEVENT_H

#include "libosns-list.h"

/* Prototypes */

void set_bevent_system_socket(struct bevent_s *bevent, struct system_socket_s *sock);
void unset_bevent_system_socket(struct bevent_s *bevent, struct system_socket_s *sock);
struct system_socket_s *get_bevent_system_socket(struct bevent_s *bevent);

struct bevent_s *create_fd_bevent(struct beventloop_s *eloop, void *ptr);
struct beventloop_s *get_eventloop_bevent(struct bevent_s *bevent);
struct bevent_s *get_next_bevent(struct beventloop_s *loop, struct bevent_s *bevent);

void set_bevent_ptr(struct bevent_s *bevent, void *ptr);
void set_bevent_cb(struct bevent_s *bevent, unsigned int flag, void (* cb)(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg));

int modify_bevent_watch(struct bevent_s *bevent);
int add_bevent_watch(struct bevent_s *bevent);
void remove_bevent_watch(struct bevent_s *bevent, unsigned int flags);

void init_bevent(struct bevent_s *bevent);
void free_bevent(struct bevent_s **p_bevent);

void disable_signal(struct bevent_argument_s *arg, unsigned int flag);

uint32_t signal_is_error(struct bevent_argument_s *a);
uint32_t signal_is_close(struct bevent_argument_s *a);
uint32_t signal_is_data(struct bevent_argument_s *a);
uint32_t signal_is_buffer(struct bevent_argument_s *a);

unsigned int printf_event_uint(struct bevent_argument_s *a);

#endif

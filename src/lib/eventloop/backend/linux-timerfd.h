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

#ifndef _LIB_EVENTLOOP_BACKEND_TIMERFD_H
#define _LIB_EVENTLOOP_BACKEND_TIMERFD_H

#include "libosns-list.h"

/* Prototypes */

int init_timerfd_backend(struct beventloop_s *loop, struct bevent_subsystem_s *subsys);

unsigned int add_timer_timerfd(struct bevent_subsystem_s *subsys, struct system_timespec_s *expire, void (* cb)(unsigned int id, void *ptr, unsigned char flags), void *ptr);
unsigned char modify_timer_timerfd(struct bevent_subsystem_s *subsys, unsigned int id, void *ptr, struct system_timespec_s *expire);
unsigned char remove_timer_timerfd(struct bevent_subsystem_s *subsys, unsigned int id, void *ptr);

#endif

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

#ifndef _LIB_EVENTLOOP_BEVENTTIMER_H
#define _LIB_EVENTLOOP_BEVENTTIMER_H

#define BTIMER_FLAG_EXPIRED				1
#define BTIMER_FLAG_REMOVED				2

/* Prototypes */

int create_bevent_timer_subsystem(struct beventloop_s *eloop);

int start_btimer_subsystem(struct beventloop_s *eloop, unsigned int id);
void stop_btimer_subsystem(struct beventloop_s *eloop, unsigned int id);
void clear_btimer_subsystem(struct beventloop_s *eloop, unsigned int id);

unsigned int add_btimer_eventloop(struct beventloop_s *eloop, unsigned int id, struct system_timespec_s *expire, void (* cb)(unsigned int id, void *ptr, unsigned char flags), void *ptr);
unsigned char modify_timer_eventloop(struct beventloop_s *eloop, unsigned int id, unsigned int timerid, struct system_timespec_s *expire);
unsigned char remove_timer_eventloop(struct beventloop_s *eloop, unsigned int id, unsigned int timerid);

#endif

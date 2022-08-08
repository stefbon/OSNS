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

#ifndef _LIB_EVENTLOOP_BEVENTSUBSYS_H
#define _LIB_EVENTLOOP_BEVENTSUBSYS_H

/* Prototypes */

struct bevent_subsystem_s *get_dummy_bevent_subsystem();

struct bevent_subsystem_s *create_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int size);
unsigned int complete_bevent_subsystem_common(struct beventloop_s *eloop, struct bevent_subsystem_s *subsys);

int start_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name);
void stop_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name);
void clear_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name);
void free_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name);

int find_bevent_subsystem(struct beventloop_s *eloop, const char *type_name);

#endif

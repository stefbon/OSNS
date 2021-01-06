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

#ifndef _COMMON_UTILS_BEVENT_H
#define _COMMON_UTILS_BEVENT_H

/* Prototypes */

void init_bevent(struct bevent_s *bevent);
struct bevent_s *get_next_bevent(struct beventloop_s *loop, struct bevent_s *bevent);

struct bevent_s *add_to_beventloop(int fd, uint32_t code, bevent_cb callback, void *data, struct bevent_s *bevent, struct beventloop_s *loop);
void remove_bevent_from_beventloop(struct bevent_s *bevent);
void modify_bevent(struct bevent_s *bevent, uint32_t code);

unsigned int set_bevent_name(struct bevent_s *xdata, char *name);
char *get_bevent_name(struct bevent_s *xdata);
int strcmp_bevent(struct bevent_s *b, char *name);

#endif

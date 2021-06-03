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

#ifndef LIB_MOUNTINFO_MONITOR_H
#define LIB_MOUNTINFO_MONITOR_H

#include "list.h"

#define MOUNTMONITOR_FLAG_INIT				1

typedef int (* update_cb_t) (uint64_t generation, struct mountentry_s *(*next) (struct mountentry_s *me, uint64_t generation, unsigned char type), void *data, unsigned char flags);
typedef unsigned char (* ignore_cb_t) (char *source, char *fs, char *path, void *data);

struct bevent_s *open_mountmonitor();
void close_mountmonitor();

void set_updatefunc_mountmonitor(update_cb_t cb);
void set_ignorefunc_mountmonitor(ignore_cb_t cb);
void set_threadsqueue_mountmonitor(void *ptr);
void set_userdata_mountmonitor(void *data);

struct mountentry_s *get_next_mountentry(struct mountentry_s *m, uint64_t generation, unsigned char type);

int lock_mountlist_read(struct simple_lock_s *lock);
int lock_mountlist_write(struct simple_lock_s *lock);
int unlock_mountlist(struct simple_lock_s *lock);

#endif

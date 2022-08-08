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

#ifndef LIB_MOUNTINFO_LIST_H
#define LIB_MOUNTINFO_LIST_H

#include "monitor.h"

#define FIND_MOUNTENTRY_FLAG_EXACT				1

/* prototypes */

int add_mountentry(struct mount_monitor_s *monitor, struct mountentry_s *me);
struct mountentry_s *find_mountentry(struct mount_monitor_s *monitor, unsigned int offset, unsigned char flags);
void process_mountentries(struct mount_monitor_s *monitor);
void clear_mountentry_lists(struct mount_monitor_s *monitor);

#endif

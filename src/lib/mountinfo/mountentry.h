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

#ifndef LIB_MOUNTINFO_MOUNTENTRY_H
#define LIB_MOUNTINFO_MOUNTENTRY_H

#include "monitor.h"

/* prototypes */

void free_mountentry_data(struct mountentry_s *me);
void free_mountentry(struct mountentry_s *me);
void release_mountentry(struct mountentry_s *me);
struct mountentry_s *create_mountentry(struct mount_monitor_s *monitor, struct mountentry_s *init);

void get_location_path_mountpoint_mountentry(struct fs_location_path_s *path, struct mountentry_s *me);

#endif

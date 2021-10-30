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

#ifndef LIB_MOUNTINFO_MOUNTINFO_H
#define LIB_MOUNTINFO_MOUNTINFO_H

#include "list.h"
#include "system.h"

#define MOUNTLIST_CURRENT		0
#define MOUNTLIST_ADDED			1
#define MOUNTLIST_REMOVED		2
#define MOUNTLIST_FSTAB			5

#define MOUNTENTRY_FLAG_AUTOFS_DIRECT	2
#define MOUNTENTRY_FLAG_AUTOFS_INDIRECT	4
#define MOUNTENTRY_FLAG_AUTOFS		6
#define MOUNTENTRY_FLAG_BY_AUTOFS	8
#define MOUNTENTRY_FLAG_REMOUNT		16
#define MOUNTENTRY_FLAG_PROCESSED	32

struct mountentry_s {
    uint64_t 				found;
    uint64_t 				generation;
    char 				*mountpoint;
    char 				*rootpath;
    char 				*fs;
    char 				*source;
    char 				*options;
    struct system_dev_s			dev;
    unsigned char 			flags;
    struct list_element_s		list_m;
    struct list_element_s		list_d;
    unsigned int			mountid;
    unsigned int			parentid;
    struct mountentry_s			*parent;
};

/* prototypes */

void increase_generation_id();
uint64_t generation_id();

int compare_mount_entries(struct mountentry_s *a, struct mountentry_s *b);
void check_mounted_by_autofs(struct mountentry_s *mountentry);

#endif

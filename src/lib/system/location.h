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

#ifndef LIB_SYSTEM_LOCATION_H
#define LIB_SYSTEM_LOCATION_H

#include "datatypes.h"

#define FS_LOCATION_FLAG_ALLOC					( 1 << 0 )
#define FS_LOCATION_FLAG_NAME					( 1 << 1 )
#define FS_LOCATION_FLAG_NAME_ALLOC				( 1 << 2 )

#define FS_LOCATION_FLAG_AT					( 1 << 7 )
#define FS_LOCATION_FLAG_PATH					( 1 << 8 )
#define FS_LOCATION_FLAG_DEVINO					( 1 << 9 )
#define FS_LOCATION_FLAG_SOCKET					( 1 << 10 )

struct fs_location_devino_s {
    dev_t							dev;
    uint64_t							ino;
};

#define FS_LOCATION_DEVINO_INIT					{0, 0}

struct fs_location_s {
    unsigned int						flags;
    union _location_u {
	struct fs_location_path_s				path;
	struct fs_location_devino_s				devino;
	struct fs_socket_s					socket;
    } type;
    char							*name;
    unsigned int						size;
    char							buffer[];
};

/* prototypes */

int get_target_unix_symlink(char *path, unsigned int len, unsigned int extra, struct fs_location_path_s *result);
int compare_fs_locations(struct fs_location_s *a, struct fs_location_s *b);

#endif

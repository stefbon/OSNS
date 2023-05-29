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

#ifndef LIB_FSPATH_FSPATH_H
#define LIB_FSPATH_FSPATH_H

#include "libosns-datatypes.h"

#define FS_PATH_FLAG_ALLOC				        1
#define FS_PATH_FLAG_BUFFER_ALLOC				2
#define FS_PATH_FLAG_RELATIVE				        4

struct fs_path_s {
#ifdef __linux__
    unsigned int						flags;
    unsigned char						back;
    unsigned int						size;
    unsigned int                                                start;
    unsigned int                                                len;
    char                                                        *buffer;
#endif
};

#ifdef __linux__

#define FS_PATH_INIT					        {0, 0, 0, 0, 0, NULL}
#define FS_PATH_SET(a, b)				        {.flags=0, .back=0, .size=(a ? a : strlen(b) + 1), .start=0, .len=0, .buffer=b}

#endif

/* prototypes */

#endif

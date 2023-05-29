/*
  2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIBOSNS_BASIC_SYSTEM_HEADERS_H
#define _LIBOSNS_BASIC_SYSTEM_HEADERS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _REENTRANT
#define _REENTRANT
#endif

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS		64

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#define OSNS_DATA_BUFFER_FLAG_ALLOC			1
#define OSNS_DATA_BUFFER_FLAG_ERROR			2

#endif

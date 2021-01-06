/*
  2010, 2011, 2012, 2103, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "logging.h"
#include "main.h"

struct custom_alloc_s {
    void				*(* alloc)(size_t size);
    void				*(* realloc)(void *ptr, size_t size);
    void				(* free)(void *ptr);
};

static struct custom_alloc_s custom_alloc;

#ifdef HAVE_LIBGCRYPT

#include "gcrypt.h"

static unsigned char securememory=1;

void *malloc_secure(size_t size)
{
    return gcry_malloc_secure(size);
}

void *realloc_secure(void *ptr, size_t size)
{
    return gcry_realloc(ptr, size);
}

void free_secure(void *ptr)
{
    gcry_free(ptr);
}

#else

static unsigned char securememory=0;

#endif

void init_custom_memory_handlers()
{

#ifdef HAVE_LIBGCRYPT

    logoutput("init_secure_memory: using libgcrypt memory handlers");

    custom_alloc.alloc=malloc_secure;
    custom_alloc.realloc=realloc_secure;
    custom_alloc.free=free_secure;

#else

    logoutput("init_secure_memory: no library available, falling back default");

    custom_alloc.alloc=malloc;
    custom_alloc.realloc=realloc;
    custom_alloc.free=free;

#endif

}

void *malloc_custom(size_t size)
{
    return custom_alloc.alloc(size);
}

void *realloc_custom(void *ptr, size_t size)
{
    return custom_alloc.realloc(ptr, size);
}

void free_custom(void *ptr)
{
    custom_alloc.free(ptr);
}

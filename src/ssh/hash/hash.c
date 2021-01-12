/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "log.h"
#include "hash.h"

#if HAVE_LIBGCRYPT

#include <gcrypt.h>

unsigned int get_hash_size(const char *name)
{
    int algo=gcry_md_map_name(name);
    return (algo>0) ? gcry_md_get_algo_dlen(algo) : 0;
}

void init_ssh_hash(struct ssh_hash_s *hash, char *name, unsigned int size)
{
    memset((char *)hash, '\0', sizeof(struct ssh_hash_s) + size);
    hash->lib.algo=gcry_md_map_name(name);
    hash->size=size;
    hash->len=0;
    strncpy(hash->name, name, sizeof(hash->name)-1);
}

unsigned int create_hash(char *in, unsigned int size, struct ssh_hash_s *hash, unsigned int *error)
{
    int algo=gcry_md_map_name(hash->name);
    gcry_md_hd_t handle;

    if (algo==0) {

	if (error) *error=EINVAL;
	return 0;

    } else if (gcry_md_open(&handle, algo, 0)==0) {
	unsigned char *digest=NULL;
	unsigned int len=gcry_md_get_algo_dlen(algo);

	gcry_md_write(handle, in, size);
	digest=gcry_md_read(handle, algo);

	if (hash->size < len) len=hash->size;
	memcpy(hash->digest, digest, len);
	hash->len=len;
	gcry_md_close(handle);
	return len;

    }

    out:

    if (error) *error=EIO;
    return 0;

}

#else

unsigned int get_hash_size(const char *name)
{
    return 0;
}
void init_ssh_hash(struct ssh_hash_s *hash, char *name, unsigned int size)
{
}

unsigned int create_hash(const char *name, char *in, unsigned int size, struct ssh_hash_s *hash, unsigned int *error)
{
    *error=EOPNOTSUPP;
    return 0;

}

#endif


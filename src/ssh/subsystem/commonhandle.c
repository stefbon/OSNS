/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include <sys/vfs.h>
#include <pwd.h>
#include <grp.h>

#include <linux/kdev_t.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "osns_sftp_subsystem.h"

#include "commonhandle.h"

static struct simple_hash_s ino_group;

void insert_commonhandle_hash(struct commonhandle_s *handle)
{
    add_data_to_hash(&ino_group, (void *) handle);
}

void remove_commonhandle_hash(struct commonhandle_s *handle)
{
    remove_data_from_hash(&ino_group, (void *) handle);
}

int writelock_commonhandles(struct simple_lock_s *lock)
{
    init_wlock_hashtable(&ino_group, lock);
    return lock_hashtable(lock);
}

int readlock_commonhandles(struct simple_lock_s *lock)
{
    init_rlock_hashtable(&ino_group, lock);
    return lock_hashtable(lock);
}

int unlock_commonhandles(struct simple_lock_s *lock)
{
    return unlock_hashtable(lock);
}

unsigned int calculate_ino_hash(uint64_t ino)
{
    unsigned int hashvalue=ino % ino_group.len;
    return hashvalue;
}

static unsigned int ino_hashfunction(void *data)
{
    struct commonhandle_s *handle=(struct commonhandle_s *) data;
    return calculate_ino_hash(handle->ino);
}

unsigned int get_filehandle_hashvalue(struct commonhandle_s *handle)
{
    return ino_hashfunction((void *) handle);
}

struct commonhandle_s *get_next_commonhandle(void **index, unsigned int hashvalue)
{
    return (struct commonhandle_s *) get_next_hashed_value(&ino_group, index, hashvalue);
}

int init_commonhandles(unsigned int *error)
{
    return initialize_group(&ino_group, ino_hashfunction, 512, error);
}

void free_commonhandles()
{
    free_group(&ino_group, NULL);
}

unsigned char write_commonhandle(struct commonhandle_s *handle, char *buffer)
{
    unsigned char pos=0;

    store_uint32(&buffer[pos], handle->dev);
    pos+=4;
    store_uint64(&buffer[pos], handle->ino);
    pos+=8;
    store_uint32(&buffer[pos], handle->pid);
    pos+=4;
    store_uint32(&buffer[pos], handle->fd);
    pos+=4;
    buffer[pos]=handle->type;
    pos++;
    return pos;
}

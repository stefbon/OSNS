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

#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "list.h"

#include "fshandle.h"

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
    return calculate_ino_hash(handle->location.type.devino.ino);
}

unsigned int get_commonhandle_hashvalue(struct commonhandle_s *handle)
{
    return ino_hashfunction((void *) handle);
}

struct commonhandle_s *get_next_commonhandle(void **index, unsigned int hashvalue)
{
    return (struct commonhandle_s *) get_next_hashed_value(&ino_group, index, hashvalue);
}

int init_hash_commonhandles(unsigned int *error)
{
    return initialize_group(&ino_group, ino_hashfunction, 512, error);
}

void free_hash_commonhandles()
{
    free_group(&ino_group, NULL);
}

struct commonhandle_s *find_commonhandle(dev_t dev, uint64_t ino, unsigned int pid, unsigned int fd, unsigned int flag, int (* compare_subsystem)(struct commonhandle_s *h, void *ptr), void *ptr)
{
    struct commonhandle_s *handle=NULL;
    unsigned int hashvalue=0;
    void *index=NULL;
    struct simple_lock_s lock;

    logoutput_debug("find_commonhandle: look for dev %i ino %i pid %i fd %i flag %i", dev, ino, pid, fd, flag);

    hashvalue=calculate_ino_hash(ino);
    readlock_commonhandles(&lock);
    handle=(struct commonhandle_s *) get_next_commonhandle(&index, hashvalue);

    while (handle) {

	logoutput_debug("find_commonhandle: handle dev %i ino %i pid %i fd %i flags %i", handle->location.type.devino.dev, handle->location.type.devino.ino, get_pid_commonhandle(handle), get_fd_commonhandle(handle), handle->flags);

	if ((handle->location.flags & FS_LOCATION_FLAG_DEVINO)) {

	    if (handle->location.type.devino.ino==ino && handle->location.type.devino.dev==dev &&
		get_pid_commonhandle(handle)==pid &&
		get_fd_commonhandle(handle)==fd &&
		(flag==0 || (handle->flags & flag)) &&
		compare_subsystem(handle, ptr)==0) break;

	}

	handle=(struct commonhandle_s *) get_next_commonhandle(&index, hashvalue);

    }

    unlock_commonhandles(&lock);
    return handle;

}

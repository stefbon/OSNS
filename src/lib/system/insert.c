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

#include "libosns-basic-system-headers.h"

#include <sys/stat.h>
#include <sys/vfs.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-network.h"

#include "fshandle.h"
#include "hash.h"
#include "file.h"
#include "directory.h"

#define ACE4_TMP_READ			( ACE4_READ_DATA | ACE4_READ_ATTRIBUTES )
#define ACE4_TMP_WRITE			( ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES )
#define ACE4_TMP_DELETE			( ACE4_DELETE )

/* does the handle block the desired access?
    TODO: if blocked give information about who has blocked... via handle->pid
    ask process with pid it's host address and uid */

static int test_desired_access(struct commonhandle_s *handle, struct commonhandle_s *new, struct insert_filehandle_s *insert)
{
    unsigned int new_access=(* new->get_access)(new);
    unsigned int existing_flags=(* handle->get_flags)(handle);

    if (new_access & ACE4_TMP_READ) {

	if (existing_flags & FILEHANDLE_BLOCK_READ) {

	    /* TODO: additional information in insert->info.lock.address/uid */

	    insert->status |= INSERTHANDLE_STATUS_DENIED_LOCK;
	    return 0;

	}

    }

    if (new_access & ACE4_TMP_WRITE) {

	if (existing_flags & FILEHANDLE_BLOCK_WRITE) {

	    /* TODO: additional information in insert->info.lock.address/uid */

	    insert->status |= INSERTHANDLE_STATUS_DENIED_LOCK;
	    return 0;

	}

    }

    if (new_access & ACE4_TMP_DELETE) {

	if (existing_flags & FILEHANDLE_BLOCK_WRITE) {

	    /* TODO: additional information in insert->info.lock.address/uid */

	    insert->status |= INSERTHANDLE_STATUS_DENIED_LOCK;
	    return 0;

	}

    }

    return 1;

}

/* does the desired lock conflict with existing access ? */

static int test_desired_flags(struct commonhandle_s *handle, struct commonhandle_s *new, struct insert_filehandle_s *insert)
{
    unsigned int existing_access=(* handle->get_access)(handle);
    unsigned int new_flags=(* new->get_flags)(new);

    if (new_flags & FILEHANDLE_BLOCK_READ) {

	/* test existing handle has read access */

	if (existing_access & ACE4_TMP_READ) {

	    insert->status |= INSERTHANDLE_STATUS_DENIED_LOCK;
	    return 0;

	}

    }

    if (new_flags & FILEHANDLE_BLOCK_WRITE) {

	/* test existing_handle has write access */

	if (existing_access & ACE4_TMP_WRITE) {

	    insert->status |= INSERTHANDLE_STATUS_DENIED_LOCK;
	    return 0;

	}

    }

    if (new_flags & FILEHANDLE_BLOCK_DELETE) {

	/* test existing_handle has delete rights */

	if (existing_access & ACE4_TMP_DELETE) {

	    insert->status |= INSERTHANDLE_STATUS_DENIED_LOCK;
	    return 0;

	}

    }

    return 1;

}


/* function to test handle will block the desired access and desired (b)lock */

static int handle_compare_for_block(struct commonhandle_s *handle, struct commonhandle_s *new, struct insert_filehandle_s *insert)
{
    int result=1;

    /* look at access and flags etc */

    if (test_desired_access(handle, new, insert)==0) {

	logoutput("handle_check_access: access denied by existing handle");
	result=0;
	goto out;

    }

    if (test_desired_flags(handle, new, insert)==0) {

	logoutput("handle_check_access: flags denied by existing handle");
	result=0;
	goto out;

    }

    out:
    return result;

}

static struct commonhandle_s *check_block_new_handle(struct commonhandle_s *new,  struct insert_filehandle_s *insert)
{
    struct commonhandle_s *handle=NULL;
    unsigned int hashvalue=0;
    void *index=NULL;
    int lockflags=0;

    hashvalue=calculate_ino_hash(new->location.type.devino.ino);
    handle=get_next_commonhandle(&index, hashvalue);

    while (handle) {

	if ((handle->flags & COMMONHANDLE_FLAG_FILE)==0) goto next;

	if (insert->type==INSERTHANDLE_TYPE_CREATE) {

	    /* when creating a file another handle with the same location is enough to block */

	    if (compare_fs_locations(&handle->location, &new->location)==0) break;

	} else if (insert->type==INSERTHANDLE_TYPE_OPEN) {

	    if (compare_fs_locations(&new->location, &handle->location)==0) {

		/* same location: block on access and lock flags */

		if (handle_compare_for_block(handle, new, insert)<1) break;

	    }

	}

	next:
	handle=get_next_commonhandle(&index, hashvalue);

    }

    return handle;
}

/* insert a file handle */

int start_insert_filehandle(struct commonhandle_s *new, struct insert_filehandle_s *insert)
{
    struct commonhandle_s *handle=NULL;
    struct osns_lock_s lock;
    int result=-1;

    if ((new->flags & COMMONHANDLE_FLAG_FILE)==0) return -1;

    logoutput("start_insert_filehandle: dev %i ino %li", new->location.type.devino.dev, (long) new->location.type.devino.ino);

    writelock_commonhandles(&lock);

    handle=check_block_new_handle(new, insert);

    if (handle==NULL) {

	/* no handle found: no conflict, so insert in hashtable */

	insert_commonhandle_hash(new);
	enable_filehandle(&new->type.file);
	result=0;

    } else {

	logoutput("start_insert_filehandle: existing handle found which blocks");

    }

    unlock_commonhandles(&lock);
    return result;

}

void complete_create_filehandle(struct commonhandle_s *handle, dev_t dev, uint64_t ino)
{

    if (handle->flags & COMMONHANDLE_FLAG_CREATE) {
	struct osns_lock_s lock;
	struct fs_location_s *location=&handle->location;

	/* rehash since the devino and fd are available */

	writelock_commonhandles(&lock);
	remove_commonhandle_hash(handle);

	if (location->name) {

	    if (location->flags & FS_LOCATION_FLAG_NAME_ALLOC) {

		free(location->name);
		location->flags &= ~FS_LOCATION_FLAG_NAME_ALLOC;

	    }

	    location->name=NULL;

	}

	/* wipe out any previous location flags used to create the file, since the method from now on is devino  */

	location->flags &= ~(FS_LOCATION_FLAG_AT | FS_LOCATION_FLAG_PATH | FS_LOCATION_FLAG_SOCKET);

	location->type.devino.dev=dev;
	location->type.devino.ino=ino;
	location->flags |= FS_LOCATION_FLAG_DEVINO;

	handle->flags &= ~COMMONHANDLE_FLAG_CREATE;

	insert_commonhandle_hash(handle);
	unlock_commonhandles(&lock);

    }

}

/* insert a dir handle */

void insert_dirhandle(struct commonhandle_s *new)
{
    struct osns_lock_s lock;

    if ((new->flags & COMMONHANDLE_FLAG_DIR)==0) return;

    logoutput("start_insert_dirhandle: dev %i ino %li", new->location.type.devino.dev, (long) new->location.type.devino.ino);

    writelock_commonhandles(&lock);

    insert_commonhandle_hash(new);
    enable_dirhandle(&new->type.dir);

    unlock_commonhandles(&lock);

}

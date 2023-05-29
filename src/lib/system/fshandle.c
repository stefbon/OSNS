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

#include <fcntl.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-list.h"

#include "fshandle.h"

/*
    every handle has the form:
    dev || inode || pid || fd || type

    - 4 bytes                           connection id
    - 4 bytes                           major
    - 4 bytes				minor
    - 8 bytes				inode

    ---------
    20 bytes

    - encrypt and decrypt? or hash with some key?

    better:

    - 1 byte				length excluding this byte self
    - 1 byte				version
    - 1 byte				padding [n]
    - 4 bytes				dev
    - 8 bytes				ino
    - 4 bytes				pid
    - 4 bytes				fd
    - 1 byte				type
    - n bytes				random 

*/

#define FS_HANDLE_HASHTABLE_SIZE        128
static struct list_header_s hashtable[FS_HANDLE_HASHTABLE_SIZE];

void init_fs_handle_hashtable()
{
    for (unsigned int i=0; i<FS_HANDLE_HASHTABLE_SIZE; i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
}

static struct fs_handle_s *find_fs_handle(unsigned int connectionid, unsigned int major, unsigned int minor, uint64_t ino)
{
    struct fs_handle_s *handle=NULL;
    unsigned int hashvalue=(ino % FS_HANDLE_HASHTABLE_SIZE);
    struct list_element_s *list=NULL;
    struct list_header_s *h=&hashtable[hashvalue];

    read_lock_list_header(h);
    list=get_list_head(h);

    while (list) {

        handle=(struct fs_handle_s *)((char *) list - offsetof(struct fs_handle_s, list));
        if ((handle->connectionid==connectionid) && (handle->ino==ino) && (handle->dev==makedev(major, minor))) break;
        handle=NULL;
        list=get_next_element(list);

    }

    read_unlock_list_header(h);
    return handle;

}

struct fs_handle_s *get_fs_handle(unsigned int connectionid, char *buffer, unsigned int size, unsigned int *p_count)
{
    unsigned char pos=0; /* unsigned char is enough for buffer size */
    unsigned int major=0;
    unsigned int minor=0;
    uint64_t ino=0;

    if (size < 20) {

	logoutput_debug("get_fs_handle: size %u too small (at least %u)", size, 20);
	return NULL;

    }

    /* read the buffer, assume it's big enough */
    connectionid=get_uint32(&buffer[pos]);
    pos+=4;
    major=get_uint32(&buffer[pos]);
    pos+=4;
    minor=get_uint32(&buffer[pos]);
    pos+=4;
    ino=get_uint64(&buffer[pos]);
    pos+=8;

    if (p_count) *p_count+=pos;
    return find_fs_handle(connectionid, major, minor, ino);
}

void free_fs_handle(struct fs_handle_s **p_handle)
{
    struct fs_handle_s *handle=(p_handle) ? *p_handle : NULL;
    struct fs_socket_s *sock=NULL;
    unsigned int flags=0;

    if (handle==NULL) return;
    sock=&handle->socket;
    flags=handle->flags;

    if (handle->list.h) {
        struct list_header_s *h=handle->list.h;

        write_lock_list_header(h);
        remove_list_element(&handle->list);
        write_unlock_list_header(h);

    }

    (* sock->clear)(sock);
    memset(handle, 0, sizeof(struct fs_handle_s));

    if (flags & HANDLE_FLAG_ALLOC) {

	free(handle);
	*p_handle=NULL;

    }

}

static unsigned int _get_access_default(struct fs_handle_s *handle)
{
    return ((handle->type == FS_HANDLE_TYPE_DIR) ? 1 : 0);
}

static unsigned int _get_flags_default(struct fs_handle_s *handle)
{
    return 0;
}

static unsigned int write_handle_default(struct fs_handle_s *handle, char *buffer, unsigned int size)
{
    unsigned int pos=0;

    /* write a handle to a buffer */

    if (size < 20) {

        pos=20;

    } else {

        store_uint32(&buffer[pos], handle->connectionid);
        pos+=4;
        store_uint32(&buffer[pos], major(handle->dev));
        pos+=4;
        store_uint32(&buffer[pos], minor(handle->dev));
        pos+=4;
        store_uint64(&buffer[pos], handle->ino);
        pos+=8;

    }

    return pos;

}

unsigned int get_fs_handle_buffer_size()
{
    return 20;
}

struct fs_handle_s *create_fs_handle(unsigned char type, unsigned int size)
{
    struct fs_handle_s *handle=malloc(sizeof(struct fs_handle_s) + size);

    if (handle==NULL) return NULL;
    memset(handle, 0, sizeof(struct fs_handle_s) + size);
    handle->flags=HANDLE_FLAG_ALLOC;
    handle->size=size;

    handle->get_access=_get_access_default;
    handle->get_flags=_get_flags_default;
    handle->write_handle=write_handle_default;

    switch (type) {

	case FS_HANDLE_TYPE_DIR:

	    handle->type = FS_HANDLE_TYPE_DIR;
	    init_fs_socket(&handle->socket, FS_SOCKET_TYPE_DIR);
	    break;

	case FS_HANDLE_TYPE_FILE:

	    handle->type = FS_HANDLE_TYPE_FILE;
	    init_fs_socket(&handle->socket, FS_SOCKET_TYPE_FILE);
	    break;

    }

    return handle;

}

void insert_fs_handle(struct fs_handle_s *handle, unsigned int connectionid, dev_t dev, uint64_t ino)
{
    unsigned int hashvalue=(ino % FS_HANDLE_HASHTABLE_SIZE);
    struct list_header_s *h=&hashtable[hashvalue];

    handle->connectionid=connectionid;
    handle->dev=dev;
    handle->ino=ino;

    write_lock_list_header(h);
    add_list_element_first(h, &handle->list);
    write_unlock_list_header(h);

}

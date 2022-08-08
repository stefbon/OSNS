/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "libosns-log.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "receive.h"
#include "request.h"

#define FUSE_HANDLE_HASHTABLE_SIZE			73
static struct list_header_s hashtable[FUSE_HANDLE_HASHTABLE_SIZE];
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

void init_fuse_handle_hashtable()
{
    for (unsigned int i=0; i<FUSE_HANDLE_HASHTABLE_SIZE; i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
}

struct fuse_handle_s *get_fuse_handle(struct service_context_s *ctx, uint64_t ino)
{
    unsigned int hashvalue=(ino % FUSE_HANDLE_HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];
    struct list_element_s *list=NULL;
    struct fuse_handle_s *handle=NULL;

    read_lock_list_header(header);
    list=get_list_head(header, 0);

    while (list) {

	handle=(struct fuse_handle_s *)((char *)list - offsetof(struct fuse_handle_s, list));

	if (handle->ino==ino && handle->ctx==ctx && (handle->flags & FUSE_HANDLE_FLAG_RELEASE)==0) {

	    pthread_mutex_lock(&mutex);
	    handle->refcount++;
	    pthread_mutex_unlock(&mutex);
	    break;

	}

	list=get_next_element(list);
	handle=NULL;

    }

    read_unlock_list_header(header);
    return handle;
}

void post_fuse_handle(struct fuse_handle_s *handle, unsigned int flag)
{
    unsigned int hashvalue=(handle->ino % FUSE_HANDLE_HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];
    unsigned char dofree=0;

    flag &= FUSE_HANDLE_FLAG_RELEASE;

    write_lock_list_header(header);

    pthread_mutex_lock(&mutex);
    handle->refcount--;
    handle->flags |= flag;
    pthread_mutex_unlock(&mutex);

    if ((handle->flags & FUSE_HANDLE_FLAG_RELEASE) && (handle->refcount==0)) {

	remove_list_element(&handle->list);
	dofree=1;

    }

    write_unlock_list_header(header);
    if (dofree) free(handle);

}

void release_fuse_handle(struct fuse_handle_s *handle)
{
    post_fuse_handle(handle, FUSE_HANDLE_FLAG_RELEASE);
}

struct fuse_handle_s *create_fuse_handle(struct service_context_s *ctx, uint64_t ino, unsigned int type, char *name, unsigned int len, uint64_t fh)
{
    struct fuse_handle_s *handle=NULL;

    if (name==NULL && len>0) {

	logoutput_warning("create_fuse_handle: invalid parameters ... name is not defined but len not zero");
	len=0;

    }

    handle=malloc(sizeof(struct fuse_handle_s) + len);

    if (handle) {
	unsigned int hashvalue=(ino % FUSE_HANDLE_HASHTABLE_SIZE);
	struct list_header_s *header=&hashtable[hashvalue];

	memset(handle, 0, sizeof(struct fuse_handle_s) + len);

	handle->ctx=ctx;
	handle->ino=ino;
	init_list_element(&handle->list, NULL);
	handle->refcount=1;
	handle->flags=((type==FUSE_HANDLE_FLAG_OPENFILE) ? FUSE_HANDLE_FLAG_OPENFILE : FUSE_HANDLE_FLAG_OPENDIR);
	handle->fh=fh;
	handle->len=len;
	if (name) memcpy(handle->name, name, len);

	write_lock_list_header(header);
	add_list_element_first(header, &handle->list);
	write_unlock_list_header(header);

    }

    return handle;

}

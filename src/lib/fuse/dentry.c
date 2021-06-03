/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "dentry.h"
#include "log.h"
#include "datatypes.h"

#define NAMEINDEX_ROOT1						92			/* number of valid chars*/
#define NAMEINDEX_ROOT2						8464			/* 92 ^ 2 */
#define NAMEINDEX_ROOT3						778688			/* 92 ^ 3 */
#define NAMEINDEX_ROOT4						71639296		/* 92 ^ 4 */
#define NAMEINDEX_ROOT5						6590815232		/* 92 ^ 5 */

extern void use_virtual_fs(struct inode_s *inode);
static struct list_header_s cacheheader=INIT_LIST_HEADER;
static pthread_mutex_t cachemutex=PTHREAD_MUTEX_INITIALIZER;

void init_data_link(struct data_link_s *link)
{
    memset(link, 0, sizeof(struct data_link_s));
    link->type=0;
}

void calculate_nameindex(struct name_s *name)
{
    char buffer[6];
    unsigned char count=(name->len > 5) ? 6 : name->len;

    memset(buffer, 32, 6);
    memcpy(buffer, name->name, count);

    unsigned char firstletter		= buffer[0] - 32;
    unsigned char secondletter		= buffer[1] - 32;
    unsigned char thirdletter		= buffer[2] - 32;
    unsigned char fourthletter		= buffer[3] - 32;
    unsigned char fifthletter		= buffer[4] - 32;
    unsigned char sixthletter		= buffer[5] - 32;

    name->index=(firstletter * NAMEINDEX_ROOT5) + (secondletter * NAMEINDEX_ROOT4) + (thirdletter * NAMEINDEX_ROOT3) + (fourthletter * NAMEINDEX_ROOT2) + (fifthletter * NAMEINDEX_ROOT1) + sixthletter;

}

void init_entry(struct entry_s *entry)
{
    entry->inode=NULL;
    entry->name.name=&entry->buffer[0];
    entry->name.len=0;
    entry->name.index=0;
    init_list_element(&entry->list, NULL);
    init_data_link(&entry->link);
    entry->flags=0;
    entry->ops=NULL;
}

/*
    allocate an entry and name

    TODO: for the name is now a seperate pointer used entry->name.name, but it would
    be better to use an array for the name
*/

struct entry_s *create_entry(struct name_s *xname)
{
    struct entry_s *entry=NULL;
    unsigned int size=sizeof(struct entry_s) + xname->len + 1;

    entry = malloc(size);

    if (entry) {

	memset(entry, 0, size); /* ensures also a terminating null byte to the name buffer */
	init_entry(entry);
	memcpy(&entry->buffer[0], xname->name, xname->len);
	entry->name.name=&entry->buffer[0];
	entry->name.len=xname->len;
	entry->name.index=xname->index;

    }

    return entry;

}

void destroy_entry(struct entry_s *entry)
{
    free(entry);
}

int check_create_inodecache(struct inode_s *inode, unsigned int size, char *buffer, unsigned int flag)
{
    struct inodecache_s *cache=NULL;
    int result=0;

    logoutput_debug("check_create_inodecache: size %i flag %i", size, flag);

    pthread_mutex_lock(&cachemutex);

    cache=inode->cache;

    if (cache) {
	struct ssh_string_s *tmp=NULL;
	struct ssh_string_s *copy=NULL;

	logoutput_debug("check_create_inodecache: exist");

	switch (flag) {

	    case INODECACHE_FLAG_STAT:

		tmp=&cache->stat;
		copy=&cache->readdir;
		break;

	    case INODECACHE_FLAG_READDIR:

		tmp=&cache->readdir;
		copy=&cache->stat;
		break;

	    case INODECACHE_FLAG_XATTR:

		tmp=&cache->xattr;
		break;

	    default:

		return 0;

	}

	if (tmp==NULL || (tmp->len==0 && size==0)) return -1;


	logoutput_debug("check_create_inodecache: size buffer %i size cache %i", size, tmp->len);

	if (tmp->len == size) {

	    if (memcmp(tmp->ptr, buffer, size) != 0) {

		/* differ */

		logoutput_debug("check_create_inodecache: memcmp");
		memcpy(tmp->ptr, buffer, size);
		result=1;

	    }

	} else {

	    if (copy && copy->len>0 && tmp->len==0 && (cache->flags & INODECACHE_FLAG_STAT_EQUAL_READDIR)==0) {

		if (memcmp(copy->ptr, buffer, 4)==0) {

		    cache->flags |= INODECACHE_FLAG_STAT_EQUAL_READDIR;
		    tmp->ptr=copy->ptr;
		    tmp->len=copy->len;

		    if (copy->len==size) {

			if (memcmp(copy->ptr, buffer, size) != 0) {

			    memcpy(copy->ptr, buffer, size);
			    result=1;

			}

		    } else {

			copy->ptr = realloc(copy->ptr, size);

			if (copy->ptr) {

			    memcpy(copy->ptr, buffer, size);
			    copy->len=size;
			    result=1;

			}

		    }

		    goto unlock;

		}

	    }

	    char *ptr=tmp->ptr;
	    unsigned char equal=(unsigned char) ((copy) ? (copy->ptr==tmp->ptr) : 0);

	    ptr=realloc(ptr, size);

	    if (ptr==NULL) {

		/* not fatal */

		tmp->len=0;
		tmp->ptr=NULL;

	    } else {

		memcpy(ptr, buffer, size);
		tmp->len=size;
		tmp->ptr=ptr;

		if (equal) {

		    copy->ptr=tmp->ptr;
		    copy->len=tmp->len;

		}

	    }

	}

    } else {
	char *ptr=NULL;

	logoutput_debug("check_create_inodecache: create");

	/* create */

	cache=malloc(sizeof(struct inodecache_s));
	ptr=malloc(size);

	if (cache && ptr) {
	    struct ssh_string_s *tmp=NULL;

	    switch (flag) {

		case INODECACHE_FLAG_STAT:

		    tmp=&cache->stat;
		    break;

		case INODECACHE_FLAG_READDIR:

		    tmp=&cache->readdir;
		    break;

		case INODECACHE_FLAG_XATTR:

		    tmp=&cache->xattr;
		    break;

	    }

	    memset(cache, 0, sizeof(struct inodecache_s));

	    cache->ino=inode->st.st_ino;
	    init_list_element(&cache->list, NULL);
	    add_list_element_first(&cacheheader, &cache->list);
	    inode->cache=cache;

	    memcpy(ptr, buffer, size);
	    tmp->ptr=ptr;
	    tmp->len=size;

	    result=1;

	} else {

	    if (cache) free(cache);
	    if (ptr) free(ptr);

	}

    }

    unlock:

    pthread_mutex_unlock(&cachemutex);
    return result;

}

void init_inode(struct inode_s *inode)
{
    struct stat *st=&inode->st;

    inode->flags=0;
    init_list_element(&inode->list, NULL);

    inode->alias=NULL;
    inode->nlookup=0;

    st->st_ino=0;
    st->st_mode=0;
    st->st_nlink=0;
    st->st_uid=(uid_t) -1;
    st->st_gid=(gid_t) -1;
    st->st_size=0;

    /* used for context->unique */
    st->st_rdev=0;
    st->st_dev=0;

    st->st_blksize=0;
    st->st_blocks=0;

    st->st_mtim.tv_sec=0;
    st->st_mtim.tv_nsec=0;
    st->st_ctim.tv_sec=0;
    st->st_ctim.tv_nsec=0;
    st->st_atim.tv_sec=0;
    st->st_atim.tv_nsec=0;

    /* synctime */
    inode->stim.tv_sec=0;
    inode->stim.tv_nsec=0;

    /* data link */
    init_data_link(&inode->link);

    inode->cache=NULL;
    inode->fs=NULL;

}

void get_inode_stat(struct inode_s *inode, struct stat *st)
{
    memcpy(st, &inode->st, sizeof(struct stat));
}

void fill_inode_stat(struct inode_s *inode, struct stat *st)
{
    //inode->mode=st->st_mode;
    //inode->nlink=st->st_nlink;

    inode->st.st_uid=st->st_uid;
    inode->st.st_gid=st->st_gid;

    //inode->size=st->st_size;

    inode->st.st_mtim.tv_sec=st->st_mtim.tv_sec;
    inode->st.st_mtim.tv_nsec=st->st_mtim.tv_nsec;

    inode->st.st_ctim.tv_sec=st->st_ctim.tv_sec;
    inode->st.st_ctim.tv_nsec=st->st_ctim.tv_nsec;

    inode->st.st_atim.tv_sec=st->st_atim.tv_sec;
    inode->st.st_atim.tv_nsec=st->st_atim.tv_nsec;

}

struct inode_s *create_inode()
{
    struct inode_s *inode = malloc(sizeof(struct inode_s));

    if (inode) {

	memset(inode, 0, sizeof(struct inode_s));
	init_inode(inode);

    }

    return inode;

}

void free_inode(struct inode_s *inode)
{
    free(inode);
}

void log_inode_information(struct inode_s *inode, uint64_t what)
{
    if (what & INODE_INFORMATION_OWNER) logoutput("log_inode_information: owner :%i", inode->st.st_uid);
    if (what & INODE_INFORMATION_GROUP) logoutput("log_inode_information: owner :%i", inode->st.st_gid);
    if (what & INODE_INFORMATION_NAME) {
	struct entry_s *entry=inode->alias;

	if (entry) {

	    logoutput("log_inode_information: entry name :%.*s", entry->name.len, entry->name.name);

	} else {

	    logoutput("log_inode_information: no entry");

	}

    }
    if (what & INODE_INFORMATION_NLOOKUP) logoutput("log_inode_information: nlookup :%li", inode->nlookup);
    if (what & INODE_INFORMATION_MODE) logoutput("log_inode_information: mode :%i", inode->st.st_mode);
    if (what & INODE_INFORMATION_NLINK) logoutput("log_inode_information: nlink :%i", inode->st.st_nlink);
    if (what & INODE_INFORMATION_SIZE) logoutput("log_inode_information: size :%i", inode->st.st_size);
    if (what & INODE_INFORMATION_MTIM) logoutput("log_inode_information: mtim %li.%li", inode->st.st_mtim.tv_sec, inode->st.st_mtim.tv_nsec);
    if (what & INODE_INFORMATION_CTIM) logoutput("log_inode_information: ctim %li.%li", inode->st.st_ctim.tv_sec, inode->st.st_ctim.tv_nsec);
    if (what & INODE_INFORMATION_ATIM) logoutput("log_inode_information: atim %li.%li", inode->st.st_atim.tv_sec, inode->st.st_atim.tv_nsec);
    if (what & INODE_INFORMATION_STIM) logoutput("log_inode_information: stim %li.%li", inode->stim.tv_sec, inode->stim.tv_nsec);

}

void init_dentry_once()
{
    init_list_header(&cacheheader, SIMPLE_LIST_TYPE_EMPTY, 0);
}

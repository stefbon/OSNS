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

static struct list_header_s cacheheader=INIT_LIST_HEADER;
static pthread_mutex_t cachemutex=PTHREAD_MUTEX_INITIALIZER;

void init_data_link(struct data_link_s *link)
{
    link->type=0;
    link->refcount=0;
}

void init_entry(struct entry_s *entry)
{
    entry->inode=NULL;
    entry->name.name=&entry->buffer[0];
    entry->name.len=0;
    entry->name.index=0;
    init_list_element(&entry->list, NULL);
    entry->ptr=NULL;
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

static void create_inodecache_common(struct inode_s *inode, unsigned int size, char *buffer, unsigned char nr, unsigned int flag)
{
    struct inodecache_s *cache=NULL;
    struct system_dev_s dev;
    struct ssh_string_s *tmp=NULL;
    char *ptr=NULL;

    logoutput_debug("create_inodecache_common: ino %li size %i nr %i sizeof %i", inode->stat.sst_ino, size, nr, sizeof(struct inodecache_s));

    /* create the cache for */

    cache=malloc(sizeof(struct inodecache_s));
    ptr=malloc(size);

    if (cache==NULL || ptr==NULL) {

	if (cache) free(cache);
	if (ptr) free(ptr);
	return;

    }

    tmp=&(cache->data[nr]);

    memset(cache, 0, sizeof(struct inodecache_s));
    init_ssh_string(&cache->data[0]);
    init_ssh_string(&cache->data[1]);
    init_ssh_string(&cache->data[2]);

    get_dev_system_stat(&inode->stat, &dev);
    cache->dev=get_unique_system_dev(&dev);
    cache->ino=inode->stat.sst_ino;

    init_list_element(&cache->list, NULL);
    add_list_element_first(&cacheheader, &cache->list);
    inode->cache=cache;

    memcpy(ptr, buffer, size);
    tmp->ptr=ptr;
    tmp->len=size;
    inode->flags |= flag;

}

void create_inodecache_stat(struct inode_s *inode, unsigned int size, char *buffer)
{
    pthread_mutex_lock(&cachemutex);
    create_inodecache_common(inode, size, buffer, INODECACHE_CACHE_STAT, INODE_FLAG_STAT_CACHED);
    pthread_mutex_unlock(&cachemutex);
}

void create_inodecache_readdir(struct inode_s *inode, unsigned int size, char *buffer)
{
    pthread_mutex_lock(&cachemutex);
    create_inodecache_common(inode, size, buffer, INODECACHE_CACHE_READDIR, INODE_FLAG_READDIR_CACHED);
    pthread_mutex_unlock(&cachemutex);
}

/* compare stat/readdir/xattr data with cache
    return:
    -1			error
    0 			the same
    1 			differ
*/

static int compare_inodecache_common(struct inode_s *inode, unsigned int size, char *buffer, unsigned char nr, int (* compare_cache)(struct ssh_string_s *data, unsigned int size, char *buffer, void *ptr), void *ptr, unsigned int flag)
{
    struct inodecache_s *cache=inode->cache;
    int result=-1;

    if (size==0 || buffer==NULL) return -1;
    logoutput_debug("compare_inodecache_common: ino %li size %i nr %i sizeof %i", inode->stat.sst_ino, size, nr, sizeof(struct inodecache_s));

    pthread_mutex_lock(&cachemutex);

    if (cache) {
	struct ssh_string_s *tmp=&(cache->data[nr]);

	logoutput_debug("compare_inodecache_common: A tmp len %i", tmp->len);

	if (compare_cache(tmp, size, buffer, ptr)==0) {

	    result=0;
	    goto unlock;

	}

	logoutput_debug("compare_inodecache_common: B2 realloc");
	tmp->ptr=realloc(tmp->ptr, size);

	/* cache en current differ
	    differ: replace  */

	if (tmp->ptr) {

	    logoutput_debug("compare_inodecache_common: D");

	    memcpy(tmp->ptr, buffer, size);
	    tmp->len=size;

	} else {

	    logoutput_warning("compare_inodecache_common: unable to allocate %i bytes: %i:%s", size, errno, strerror(errno));
	    tmp->len=0;

	}

    } else {

	create_inodecache_common(inode, size, buffer, nr, flag);
	result=1;

    }

    unlock:
    pthread_mutex_unlock(&cachemutex);
    return result;

}

int compare_inodecache_stat(struct inode_s *inode, unsigned int size, char *buffer, int (* compare_cache)(struct ssh_string_s *data, unsigned int size, char *buffer, void *ptr), void *ptr)
{
    return compare_inodecache_common(inode, size, buffer, INODECACHE_CACHE_STAT, compare_cache, ptr, INODE_FLAG_STAT_CACHED);
}

int compare_inodecache_readdir(struct inode_s *inode, unsigned int size, char *buffer, int (* compare_cache)(struct ssh_string_s *data, unsigned int size, char *buffer, void *ptr), void *ptr)
{
    return compare_inodecache_common(inode, size, buffer, INODECACHE_CACHE_READDIR, compare_cache, ptr, INODE_FLAG_READDIR_CACHED);
}

void init_inode(struct inode_s *inode)
{
    struct system_stat_s *stat=&inode->stat;
    struct system_dev_s dev=SYSTEM_DEV_INIT;
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    inode->flags=0;
    init_list_element(&inode->list, NULL);

    inode->alias=NULL;
    inode->nlookup=0;

    stat->sst_ino=0;
    stat->sst_mode=0;
    stat->sst_nlink=0;
    stat->sst_uid=(uid_t) -1;
    stat->sst_gid=(gid_t) -1;
    stat->sst_size=0;

    stat->sst_blksize=0;
    stat->sst_blocks=0;

    set_dev_system_stat(stat, &dev);
    set_rdev_system_stat(stat, &dev);
    set_atime_system_stat(stat, &time);
    set_mtime_system_stat(stat, &time);
    set_btime_system_stat(stat, &time);
    set_ctime_system_stat(stat, &time);

    /* synctime */
    set_system_time(&inode->stime, 0, 0);

    inode->ptr=NULL;
    inode->cache=NULL;
    inode->fs=NULL;

}

void copy_inode_stat(struct inode_s *inode, struct system_stat_s *stat)
{
    memcpy(stat, &inode->stat, sizeof(struct system_stat_s));
}

void fill_inode_stat(struct inode_s *inode, struct system_stat_s *stat)
{
    struct system_timespec_s time;

    inode->stat.sst_uid=stat->sst_uid;
    inode->stat.sst_gid=stat->sst_gid;

    copy_atime_system_stat(&inode->stat, stat);
    copy_btime_system_stat(&inode->stat, stat);
    copy_ctime_system_stat(&inode->stat, stat);
    copy_mtime_system_stat(&inode->stat, stat);

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

    if (what & INODE_INFORMATION_OWNER) logoutput("log_inode_information: owner :%i", get_uid_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_GROUP) logoutput("log_inode_information: owner :%i", get_gid_system_stat(&inode->stat));

    if (what & INODE_INFORMATION_NAME) {
	struct entry_s *entry=inode->alias;

	if (entry) {

	    logoutput("log_inode_information: entry name :%.*s", entry->name.len, entry->name.name);

	} else {

	    logoutput("log_inode_information: no entry");

	}

    }
    if (what & INODE_INFORMATION_NLOOKUP) logoutput("log_inode_information: nlookup :%li", inode->nlookup);
    if (what & INODE_INFORMATION_MODE) logoutput("log_inode_information: mode :%i", get_mode_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_NLINK) logoutput("log_inode_information: nlink :%i", get_nlink_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_SIZE) logoutput("log_inode_information: size :%i", get_size_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_MTIM) logoutput("log_inode_information: mtim %li.%li", get_mtime_sec_system_stat(&inode->stat), get_mtime_nsec_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_CTIM) logoutput("log_inode_information: ctim %li.%li", get_ctime_sec_system_stat(&inode->stat), get_ctime_nsec_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_ATIM) logoutput("log_inode_information: atim %li.%li", get_atime_sec_system_stat(&inode->stat), get_atime_nsec_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_STIM) logoutput("log_inode_information: stim %li.%li", inode->stime.st_sec, inode->stime.st_nsec);

}

void init_dentry_once()
{
    init_list_header(&cacheheader, SIMPLE_LIST_TYPE_EMPTY, 0);
}

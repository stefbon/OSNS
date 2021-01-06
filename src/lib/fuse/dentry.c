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

#define NAMEINDEX_ROOT1						92			/* number of valid chars*/
#define NAMEINDEX_ROOT2						8464			/* 92 ^ 2 */
#define NAMEINDEX_ROOT3						778688			/* 92 ^ 3 */
#define NAMEINDEX_ROOT4						71639296		/* 92 ^ 4 */
#define NAMEINDEX_ROOT5						6590815232		/* 92 ^ 5 */

void calculate_nameindex(struct name_s *name)
{
    char buffer[6];

    memset(buffer, 32, 6);

    if (name->len>5) {

	memcpy(buffer, name->name, 6);

    } else {

	memcpy(buffer, name->name, name->len);

    }

    unsigned char firstletter=*buffer-32;
    unsigned char secondletter=*(buffer+1)-32;
    unsigned char thirdletter=*(buffer+2)-32;
    unsigned char fourthletter=*(buffer+3)-32;
    unsigned char fifthletter=*(buffer+4)-32;
    unsigned char sixthletter=*(buffer+5)-32;

    name->index=(firstletter * NAMEINDEX_ROOT5) + (secondletter * NAMEINDEX_ROOT4) + (thirdletter * NAMEINDEX_ROOT3) + (fourthletter * NAMEINDEX_ROOT2) + (fifthletter * NAMEINDEX_ROOT1) + sixthletter;

}

void init_entry(struct entry_s *entry)
{
    entry->inode=NULL;
    entry->name.name=&entry->buffer[0];
    entry->name.len=0;
    entry->name.index=0;
    init_list_element(&entry->list, NULL);
    entry->flags=0;
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

	memset(entry, 0, size);
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

void init_inode(struct inode_s *inode)
{
    struct stat *st=&inode->st;

    memset(inode, 0, sizeof(struct inode_s) + inode->cache_size);

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

    inode->link.type=0;
    inode->link.link.ptr=NULL;

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

struct inode_s *create_inode(unsigned int cache_size)
{
    struct inode_s *inode=NULL;

    inode = malloc(sizeof(struct inode_s) + cache_size);

    if (inode) {

	inode->cache_size=cache_size;
	init_inode(inode);

    }

    return inode;

}

void free_inode(struct inode_s *inode)
{
    free(inode);
}

struct inode_s *realloc_inode(struct inode_s *inode, unsigned int new)
{
    struct inode_s *keep=inode;

    inode=realloc(inode, sizeof(struct inode_s) + new); /* assume always good */

    if (inode != keep) {
	struct list_element_s *next=NULL;
	struct list_element_s *prev=NULL;

	if (inode==NULL) return NULL;
	next=get_next_element(&inode->list);
	prev=get_prev_element(&inode->list);

	/* repair */

	if (next) {

	    next->p=&inode->list;

	} else {
	    struct list_header_s *header=inode->list.h;

	    /* no next so thus at tail */
	    header->tail=&inode->list;

	}

	if (prev) {

	    prev->n=&inode->list;

	} else {
	    struct list_header_s *header=inode->list.h;

	    /* no prev so thus at head */
	    header->head=&inode->list;

	}

	if (inode->alias) {

	    struct entry_s *entry=inode->alias;
	    entry->inode=inode;

	}

    }

    inode->cache_size=new;
    return inode;
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

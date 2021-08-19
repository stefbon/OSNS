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

#include "log.h"
#include "list.h"
#include "sftp/common.h"
#include "sftp/attr-context.h"
#include "write-attr-generic.h"

#define WRITEATTRCB_TABLE_SIZE		17

struct writeattrcb_s {
    struct list_element_s		list;
    uint32_t				valid;
    unsigned char			version;
    unsigned int			count;
    rw_attr_cb				cb[];
};

static struct list_header_s writeattrcbs[WRITEATTRCB_TABLE_SIZE];
static pthread_mutex_t wacb_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wacb_cond=PTHREAD_COND_INITIALIZER;
static unsigned char refcount=0;

static void add_writeattrcb_hashtable(struct writeattrcb_s *wacb)
{
    unsigned int hash = wacb->valid % WRITEATTRCB_TABLE_SIZE;
    struct list_header_s *header=&writeattrcbs[hash];

    write_lock_list_header(header, &wacb_mutex, &wacb_cond);
    add_list_element_first(header, &wacb->list);
    write_unlock_list_header(header, &wacb_mutex, &wacb_cond);
}

static void remove_writeattrcb_hashtable(struct writeattrcb_s *wacb)
{
    unsigned int hash = wacb->valid % WRITEATTRCB_TABLE_SIZE;
    struct list_header_s *header=&writeattrcbs[hash];

    write_lock_list_header(header, &wacb_mutex, &wacb_cond);
    remove_list_element(&wacb->list);
    write_unlock_list_header(header, &wacb_mutex, &wacb_cond);
}

static struct writeattrcb_s *lookup_writeattrcb(uint32_t valid, unsigned char version)
{
    unsigned int hash = valid % WRITEATTRCB_TABLE_SIZE;
    struct list_header_s *header=&writeattrcbs[hash];
    struct list_element_s *list=NULL;
    struct writeattrcb_s *wacb=NULL;

    read_lock_list_header(header, &wacb_mutex, &wacb_cond);
    list=get_list_head(header, 0);

    while (list) {

	wacb=(struct writeattrcb_s *)((char *) list - offsetof(struct writeattrcb_s, list));

	if (wacb->valid==valid && wacb->version==version) break;
	list=get_next_element(list);
	wacb=NULL;

    }

    read_unlock_list_header(header, &wacb_mutex, &wacb_cond);
    return wacb;

}

void init_rw_attr_result(struct rw_attr_result_s *r)
{
    memset(r, 0, sizeof(struct rw_attr_result_s));
}

void write_sftp_attributes_generic(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int valid=r->valid;
    struct writeattrcb_s *wacb=lookup_writeattrcb(valid, (* ctx->get_sftp_protocol_version)(ctx));
    unsigned char ctr=0;

    if (wacb) {

	/* there is already a set of cb's available for this valid and protocol version */

	while (ctr<r->count) {

	    (* wacb->cb[ctr])(ctx, buffer, r, attr);
	    ctr++;

	}

    } else {
	rw_attr_cb cb[r->count];
	unsigned int index=0;
	uint32_t flag=0;

	while (ctr<r->count && r->todo>0) {

	    /* following gives a 0 or 1 if flag is set */

	    flag=((r->valid & r->attrcb[ctr].code) >> r->attrcb[ctr].shift);

	    if (flag) {

		/* run the cb if flag 1 or the "do nothing" cb if flag 0*/

		(* r->attrcb[ctr].cb[1])(ctx, buffer, r, attr);
		cb[index]=r->attrcb[ctr].cb[1];
		index++;
		r->valid &= ~r->attrcb[ctr].code;

	    }

	    ctr++;

	}

	/* when here: the "valid attributes callbacks block" was not found, so create one */

	if (index>0) {

	    wacb=malloc(sizeof(struct writeattrcb_s) + index * sizeof(rw_attr_cb));

	    if (wacb) {

		init_list_element(&wacb->list, NULL);
		wacb->valid=valid;
		wacb->version=(* ctx->get_sftp_protocol_version)(ctx);
		wacb->count=index;

		memcpy(wacb->cb, cb, index * sizeof(rw_attr_cb));
		add_writeattrcb_hashtable(wacb);

	    } else {

		logoutput_warning("write_sftp_attributes_generic: error allocating write attr cb's (index=%i)", index);

	    }

	}

    }

}

void init_writeattr_generic()
{

    pthread_mutex_lock(&wacb_mutex);

    if (refcount==0) for (unsigned int i=0; i<WRITEATTRCB_TABLE_SIZE; i++) init_list_header(&writeattrcbs[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    refcount++;

    pthread_mutex_unlock(&wacb_mutex);

}

void clear_writeattr_generic(unsigned char force)
{

    pthread_mutex_lock(&wacb_mutex);

    if (refcount>0) {

	refcount--;
	if (force==0 && refcount==0) force=1;

    }

    if (force) {
	unsigned int hash=0;

	while (hash<WRITEATTRCB_TABLE_SIZE) {
	    struct list_header_s *header=&writeattrcbs[hash];
	    struct list_element_s *list=NULL;

	    while ((list=get_list_head(header, SIMPLE_LIST_FLAG_REMOVE))) {

		struct writeattrcb_s *wacb=(struct writeattrcb_s *)((char *) list - offsetof(struct writeattrcb_s, list));
		free(wacb);
		list=NULL;

	    }

	    hash++;

	}

    }

    pthread_mutex_unlock(&wacb_mutex);

}

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
#include "rw-attr-generic.h"

#define HASH_ATTRCB_TABLE_SIZE				17

#ifndef SSH_FILEXFER_ATTR_SUBSECOND_TIMES
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES 		1 << 8
#endif

static struct list_header_s header_attrcbs[HASH_ATTRCB_TABLE_SIZE];
static pthread_mutex_t hash_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hash_cond=PTHREAD_COND_INITIALIZER;
static unsigned char refcount=0;

static void add_attrcb_hashtable(struct hashed_attrcb_s *hcb)
{
    unsigned int hash = hcb->valid % HASH_ATTRCB_TABLE_SIZE;
    struct list_header_s *header=&header_attrcbs[hash];
    struct hashed_attrcb_s *tmp=NULL;
    struct list_element_s *list=NULL;

    write_lock_list_header(header, &hash_mutex, &hash_cond);
    list=get_list_head(header, 0);

    /* test it's not added in the meantime for sure */

    while (list) {

	tmp=(struct hashed_attrcb_s *)((char *) list - offsetof(struct hashed_attrcb_s, list));
	if (tmp->valid==hcb->valid && tmp->version==hcb->version) break;
	list=get_next_element(list);
	tmp=NULL;

    }

    if (tmp) {

	free(hcb);

    } else {

	add_list_element_first(header, &hcb->list);

    }

    write_unlock_list_header(header, &hash_mutex, &hash_cond);
}

static void remove_attrcb_hashtable(struct hashed_attrcb_s *hcb)
{
    unsigned int hash = hcb->valid % HASH_ATTRCB_TABLE_SIZE;
    struct list_header_s *header=&header_attrcbs[hash];

    write_lock_list_header(header, &hash_mutex, &hash_cond);
    remove_list_element(&hcb->list);
    write_unlock_list_header(header, &hash_mutex, &hash_cond);
}

struct hashed_attrcb_s *lookup_hashed_attrcb(uint32_t valid, unsigned char version)
{
    unsigned int hash = valid % HASH_ATTRCB_TABLE_SIZE;
    struct list_header_s *header=&header_attrcbs[hash];
    struct list_element_s *list=NULL;
    struct hashed_attrcb_s *hcb=NULL;

    read_lock_list_header(header, &hash_mutex, &hash_cond);
    list=get_list_head(header, 0);

    while (list) {

	hcb=(struct hashed_attrcb_s *)((char *) list - offsetof(struct hashed_attrcb_s, list));

	if (hcb->valid==valid && hcb->version==version) break;
	list=get_next_element(list);
	hcb=NULL;

    }

    read_unlock_list_header(header, &hash_mutex, &hash_cond);
    return hcb;

}

void init_rw_attr_result(struct rw_attr_result_s *r)
{
    memset(r, 0, sizeof(struct rw_attr_result_s));
}

void create_hashed_attrcb(unsigned char version, unsigned int valid, unsigned char *cb, unsigned char count, unsigned int len, unsigned int stat_mask)
{
    unsigned int size=sizeof(struct hashed_attrcb_s) + count * sizeof(unsigned char);
    struct hashed_attrcb_s *hcb=malloc(size);

    if (hcb) {

	memset(hcb, 0, size);

	init_list_element(&hcb->list, NULL);
	hcb->valid=valid;
	hcb->version=version;
	hcb->count=count;
	hcb->len=len;
	hcb->stat_mask=stat_mask;

	/* copy the array with the right rows/indices of attrcb */

	memcpy(hcb->cb, cb, count * sizeof(unsigned char));
	add_attrcb_hashtable(hcb);

    } else {

	logoutput_warning("create_hashed_attrcb: error allocating hashed attr cb's (count=%i)", count);

    }

}

void parse_attributes_generic(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid)
{
    unsigned char version=(* actx->get_sftp_protocol_version)(actx);
    struct hashed_attrcb_s *hcb=NULL;
    unsigned char ctr=0;

    /* take only the valid bits which are supported */

    if (r->flags & RW_ATTR_RESULT_FLAG_WRITE) {

	r->valid = (valid & actx->w_valid);

    } else if (r->flags & RW_ATTR_RESULT_FLAG_READ) {

	r->valid = (valid & actx->r_valid);

    }

    r->ignored = (valid &= ~r->valid);				/* which attributes are not taken into account */
    r->todo = (r->valid & ~SSH_FILEXFER_ATTR_SUBSECOND_TIMES);	/* the flags of the main attributes */
    r->done=0;

    hcb=lookup_hashed_attrcb(r->valid, version);

    if (hcb) {
	unsigned char index=0;

	/* there is already a set of cached cb's available for this valid and protocol version */

	r->flags |= RW_ATTR_RESULT_FLAG_CACHED;

	while (index<hcb->count && r->todo>0) {

	    /* the ctr is the indexnr of the arrayelement attrcb */

	    ctr=hcb->cb[index];
	    (* r->parse_attribute)(actx, buffer, r, stat, ctr);
	    index++;

	}

    } else {

	(* actx->ops.parse_attributes)(actx, buffer, r, stat);

    }

}

static void _attr_read_cb(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
    r->stat_mask |= actx->attrcb[ctr].stat_mask;
    r->done |= actx->attrcb[ctr].code;
    r->todo &= ~actx->attrcb[ctr].code;
    (* actx->attrcb[ctr].r_cb)(actx, buffer, r, stat);
}

void read_attributes_generic(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid)
{
    r->flags |= RW_ATTR_RESULT_FLAG_READ;
    r->parse_attribute=_attr_read_cb;
    parse_attributes_generic(actx, buffer, r, stat, valid);
}

static void _attr_write_cb(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
    r->stat_mask |= actx->attrcb[ctr].stat_mask;
    r->done |= actx->attrcb[ctr].code;
    r->todo &= ~actx->attrcb[ctr].code;
    (* actx->attrcb[ctr].w_cb)(actx, buffer, r, stat);
    logoutput_debug("_attr_write_cb: ctr %i code %i done %i todo %i", ctr, actx->attrcb[ctr].code, r->done, r->todo);
}

void write_attributes_generic(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid)
{
    r->flags |= RW_ATTR_RESULT_FLAG_WRITE;
    r->parse_attribute=_attr_write_cb;
    parse_attributes_generic(actx, buffer, r, stat, valid);
}

struct _prepare_write_s {
    unsigned char			*cb;
    unsigned char			index;
    unsigned int			len;
};

static void _prepare_write_cb(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
    struct _prepare_write_s *tmp=(struct _prepare_write_s *) r->ptr;

    tmp->cb[tmp->index]=ctr;
    tmp->len += actx->attrcb[ctr].maxlength;
    tmp->index++;

    r->stat_mask |= actx->attrcb[ctr].stat_mask;

    r->done |= actx->attrcb[ctr].code;
    r->todo &= ~actx->attrcb[ctr].code;

}

unsigned int get_size_buffer_write_attributes(struct attr_context_s *actx, struct rw_attr_result_s *r, unsigned int valid)
{
    unsigned char cb[36];
    struct _prepare_write_s tmp;

    tmp.cb=cb;
    tmp.index=0;
    tmp.len=0;

    r->flags |= RW_ATTR_RESULT_FLAG_WRITE;
    r->parse_attribute=_prepare_write_cb;
    r->ptr=(void *) &tmp;

    parse_attributes_generic(actx, NULL, r, NULL, valid);

    /* only if there are attributes done AND not already in cache create a new hashed attribute block */

    if (tmp.index>0 && (r->flags & RW_ATTR_RESULT_FLAG_CACHED)==0) {
	unsigned char version=(* actx->get_sftp_protocol_version)(actx);

	create_hashed_attrcb(version, valid, cb, tmp.index, tmp.len, r->stat_mask);

    }

    return tmp.len;

}

static void _read_stat_mask_cb(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
    struct _read_mask_s *tmp=(struct _read_mask_s *) r->ptr;

    r->stat_mask |= actx->attrcb[ctr].stat_mask;
}

unsigned int translate_valid_2_stat_mask(struct attr_context_s *actx, unsigned int valid, unsigned char what)
{
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;

    if (what=='r') {

	r.flags |= RW_ATTR_RESULT_FLAG_READ;

    } else if (what=='w') {

	r.flags |= RW_ATTR_RESULT_FLAG_WRITE;

    }

    r.parse_attribute=_read_stat_mask_cb;
    parse_attributes_generic(actx, NULL, &r, NULL, valid);

    return r.stat_mask;

}

struct _get_valid_flags_s {
    unsigned int 			mask;
    unsigned int			valid;
};

static void _get_valid_flags_cb(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
    struct _get_valid_flags_s *tmp=(struct _get_valid_flags_s *) r->ptr;

    if (actx->attrcb[ctr].stat_mask & tmp->mask) tmp->valid |= actx->attrcb[ctr].code;
    r->todo &= ~actx->attrcb[ctr].code;
}

unsigned int translate_stat_mask_2_valid(struct attr_context_s *actx, unsigned int mask, unsigned char what)
{
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct _get_valid_flags_s tmp;
    unsigned int valid=get_supported_valid_flags(actx, what);

    if (what=='r') {

	r.flags |= RW_ATTR_RESULT_FLAG_READ;

    } else if (what=='w') {

	r.flags |= RW_ATTR_RESULT_FLAG_WRITE;

    }

    tmp.mask=mask;
    tmp.valid=0;

    r.parse_attribute=_get_valid_flags_cb;
    r.ptr=(void *) &tmp;
    parse_attributes_generic(actx, NULL, &r, NULL, valid);

    return tmp.valid;

}

struct _parse_stat_mask_s {
    void			(* cb)(unsigned int stat_mask, unsigned int len, unsigned int validflag, unsigned int fattr, void *ptr);
    void			*ptr;
};

static void _parse_stat_mask_cb(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
    struct _parse_stat_mask_s *psm=(struct _parse_stat_mask_s *) r->ptr;

    (* psm->cb)(actx->attrcb[ctr].stat_mask, actx->attrcb[ctr].maxlength, actx->attrcb[ctr].code, actx->attrcb[ctr].fattr, psm->ptr);

    r->stat_mask |= actx->attrcb[ctr].stat_mask;
    r->done |= actx->attrcb[ctr].code;
    r->todo &= ~actx->attrcb[ctr].code;

}

void parse_sftp_attributes_stat_mask(struct attr_context_s *actx, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char what, void (* cb)(unsigned int stat_mask, unsigned int len, unsigned int validflag, unsigned int fattr, void *ptr), void *ptr)
{
    unsigned int valid=get_supported_valid_flags(actx, what);
    struct _parse_stat_mask_s psm;

    if (what=='r') {

	r->flags |= RW_ATTR_RESULT_FLAG_READ;

    } else if (what=='w') {

	r->flags |= RW_ATTR_RESULT_FLAG_WRITE;

    }

    psm.cb=cb;
    psm.ptr=ptr;

    r->parse_attribute=_parse_stat_mask_cb;
    r->ptr=(void *) &psm;

    parse_attributes_generic(actx, NULL, r, stat, valid);
}

void init_hashattr_generic()
{

    pthread_mutex_lock(&hash_mutex);

    if (refcount==0) for (unsigned int i=0; i<HASH_ATTRCB_TABLE_SIZE; i++) init_list_header(&header_attrcbs[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    refcount++;

    pthread_mutex_unlock(&hash_mutex);

}

void clear_hashattr_generic(unsigned char force)
{

    pthread_mutex_lock(&hash_mutex);

    if (refcount>0) {

	refcount--;
	if (force==0 && refcount==0) force=1;

    }

    if (force) {
	unsigned int hash=0;

	while (hash<HASH_ATTRCB_TABLE_SIZE) {
	    struct list_header_s *header=&header_attrcbs[hash];
	    struct list_element_s *list=NULL;

	    while ((list=get_list_head(header, SIMPLE_LIST_FLAG_REMOVE))) {

		struct hashed_attrcb_s *hcb=(struct hashed_attrcb_s *)((char *) list - offsetof(struct hashed_attrcb_s, list));
		free(hcb);
		list=NULL;

	    }

	    hash++;

	}

    }

    pthread_mutex_unlock(&hash_mutex);

}

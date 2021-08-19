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
#include "main.h"

#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"
#include "read-attr-v03.h"

extern unsigned char get_sftp_protocol_version_ptr(void *ptr);

#define VALIDATTRCB_TABLE_SIZE		17

struct validattrcb_s {
    struct list_element_s		list;
    uint32_t				valid;
    unsigned char			version;
    unsigned int			count;
    rw_attr_cb				cb[];
};

static struct list_header_s validattrcbs[VALIDATTRCB_TABLE_SIZE];
static pthread_mutex_t vacb_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t vacb_cond=PTHREAD_COND_INITIALIZER;
static unsigned char refcount=0;

static void add_validattrcb_hashtable(struct validattrcb_s *vacb)
{
    unsigned int hash = vacb->valid % VALIDATTRCB_TABLE_SIZE;
    struct list_header_s *header=&validattrcbs[hash];

    write_lock_list_header(header, &vacb_mutex, &vacb_cond);
    add_list_element_first(header, &vacb->list);
    write_unlock_list_header(header, &vacb_mutex, &vacb_cond);
}

static void remove_validattrcb_hashtable(struct validattrcb_s *vacb)
{
    unsigned int hash = vacb->valid % VALIDATTRCB_TABLE_SIZE;
    struct list_header_s *header=&validattrcbs[hash];

    write_lock_list_header(header, &vacb_mutex, &vacb_cond);
    remove_list_element(&vacb->list);
    write_unlock_list_header(header, &vacb_mutex, &vacb_cond);
}

static struct validattrcb_s *lookup_validattrcb(uint32_t valid, unsigned char version)
{
    unsigned int hash = valid % VALIDATTRCB_TABLE_SIZE;
    struct list_header_s *header=&validattrcbs[hash];
    struct list_element_s *list=NULL;
    struct validattrcb_s *vacb=NULL;

    read_lock_list_header(header, &vacb_mutex, &vacb_cond);
    list=get_list_head(header, 0);

    while (list) {

	vacb=(struct validattrcb_s *)((char *) list - offsetof(struct validattrcb_s, list));

	if (vacb->valid==valid && vacb->version==version) break;
	list=get_next_element(list);
	vacb=NULL;

    }

    read_unlock_list_header(header, &vacb_mutex, &vacb_cond);
    return vacb;

}

void read_sftp_attributes_generic(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned char version=(* ctx->get_sftp_protocol_version)(ctx);
    struct validattrcb_s *vacb=lookup_validattrcb(r->valid, version);
    unsigned char ctr=0;

    logoutput("read_sftp_attributes_generic: version %i valid %i", version, r->valid);

    if (vacb) {

	/* there is already a set of cb's available for this valid and protocol version */

	while (ctr<vacb->count && r->todo>0) {

	    logoutput("read_sftp_attributes_generic: vacb valid %i ctr %i flag pos %i", r->valid, ctr, (int)(buffer->pos - buffer->buffer));
	    (* vacb->cb[ctr])(ctx, buffer, r, attr);
	    ctr++;

	}

    } else {
	rw_attr_cb cb[r->count];
	unsigned int index=0;
	uint32_t flag=0;

	while (ctr<r->count && r->todo>0) {

	    /* following gives a 0 or 1 if flag is set */

	    flag=(r->todo & r->attrcb[ctr].code) >> r->attrcb[ctr].shift;

	    logoutput("read_sftp_attributes_generic: valid %i ctr %i flag %i pos %i", r->valid, ctr, flag, (int)(buffer->pos - buffer->buffer));

	    if (flag) {

		/* run the cb if flag 1 or the "do nothing" cb if flag 0*/

		(* r->attrcb[ctr].cb[1])(ctx, buffer, r, attr);
		cb[index]=r->attrcb[ctr].cb[1];
		index++;

	    }

	    ctr++;

	}

	/* when here: the "valid attributes callbacks block" was not found, so create one */

	if (index>0) {

	    vacb=malloc(sizeof(struct validattrcb_s) + index * sizeof(rw_attr_cb));

	    if (vacb) {

		init_list_element(&vacb->list, NULL);
		vacb->valid=r->valid;
		vacb->version=(* ctx->get_sftp_protocol_version)(ctx);
		vacb->count=index;

		memcpy(vacb->cb, cb, index * sizeof(rw_attr_cb));
		add_validattrcb_hashtable(vacb);

		logoutput("read_sftp_attributes_generic: added vacb valid %i version %i", r->valid, vacb->version);

	    } else {

		logoutput_warning("read_sftp_attributes_generic: error allocating valid attr cb's (index=%i)", index);

	    }

	}

    }

}

void init_readattr_generic()
{

    pthread_mutex_lock(&vacb_mutex);

    if (refcount==0) for (unsigned int i=0; i<VALIDATTRCB_TABLE_SIZE; i++) init_list_header(&validattrcbs[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    refcount++;

    pthread_mutex_unlock(&vacb_mutex);

}

void clear_readattr_generic(unsigned char force)
{

    pthread_mutex_lock(&vacb_mutex);

    if (refcount>0) {

	refcount--;
	if (force==0 && refcount==0) force=1;

    }

    if (force) {
	unsigned int hash=0;

	while (hash<VALIDATTRCB_TABLE_SIZE) {
	    struct list_header_s *header=&validattrcbs[hash];
	    struct list_element_s *list=NULL;

	    while ((list=get_list_head(header, SIMPLE_LIST_FLAG_REMOVE))) {

		struct validattrcb_s *vacb=(struct validattrcb_s *)((char *) list - offsetof(struct validattrcb_s, list));
		free(vacb);
		list=NULL;

	    }

	    hash++;

	}

    }

    pthread_mutex_unlock(&vacb_mutex);

}

/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

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
#include <sys/param.h>
#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "skiplist.h"
#include "skiplist-utils.h"
#include "log.h"

int init_sl_skiplist(struct sl_skiplist_s *sl,
		    int (* compare) (struct list_element_s *l, void *b),
		    void (* insert) (struct list_element_s *l),
		    void (* delete) (struct list_element_s *l),
		    struct list_element_s *(* get_list_element) (void *lookupdata, struct sl_skiplist_s *sl))
{
    unsigned int error=0;

    if ( ! sl) {

	error=EINVAL;

    } else if (! get_list_element || ! compare || ! insert || ! delete) {

	error=EINVAL;

    }

    if (error==0) {

	sl->ops.compare=compare;
	sl->ops.insert=insert;
	sl->ops.delete=delete;
	sl->ops.get_list_element=get_list_element;

	for (unsigned int i=0; i<=sl->maxlevel+1; i++) {

	    sl->dirnode.junction[i].n=&sl->dirnode;
	    sl->dirnode.junction[i].p=&sl->dirnode;

	}
	return 0;

    }

    logoutput("init_sl_skiplist: error %i (%s)", error, strerror(error));
    return -1;

}

struct sl_skiplist_s *create_sl_skiplist(struct sl_skiplist_s *sl, unsigned char prob, unsigned char maxlanes)
{
    unsigned int tmp=(prob==0) ? _SKIPLIST_PROB : prob;
    unsigned char lanes=(maxlanes>0) ? maxlanes : _SKIPLIST_MAXLANES;
    unsigned int size=sizeof(struct sl_skiplist_s) + (lanes + 2)  * sizeof(struct sl_junction_s);

    if (sl==NULL) {

	sl=malloc(size);
	if (sl==NULL) return NULL;
	memset(sl, 0, size);
	sl->flags=_SKIPLIST_FLAG_ALLOC;

    }

    sl->prob=tmp;
    pthread_mutex_init(&sl->mutex, NULL);
    pthread_cond_init(&sl->cond, NULL);
    sl->maxlevel=lanes + 1;
    init_list_header(&sl->header, SIMPLE_LIST_TYPE_EMPTY, NULL);

    logoutput("create_sl_skiplist: ml %i", lanes);

    _init_sl_dirnode(sl, &sl->dirnode, sl->maxlevel, _DIRNODE_FLAG_START);
    sl->dirnode.level=0;
    return sl;

}

unsigned int get_size_sl_skiplist(unsigned char *p_maxlanes)
{
    unsigned char maxlanes=*p_maxlanes;
    unsigned int size=0;

    if (maxlanes==0) {

	maxlanes=_SKIPLIST_MAXLANES;
	*p_maxlanes=maxlanes;

    }

    size=sizeof(struct sl_skiplist_s) + (maxlanes+2) * sizeof(struct sl_junction_s);
    logoutput("get_size_sl_skiplist: size %i = %i + (%i + 2) x %i" , size, sizeof(struct sl_skiplist_s), maxlanes, sizeof(struct sl_junction_s));

    return size;
}

void clear_sl_skiplist(struct sl_skiplist_s *sl)
{
    struct sl_dirnode_s *dirnode=sl->dirnode.junction[0].n;

    while (dirnode) {
	struct sl_dirnode_s *next=dirnode->junction[0].n; /* remind the next */

	free(dirnode);
	dirnode=next;

    }

}

void free_sl_skiplist(struct sl_skiplist_s *sl)
{
    clear_sl_skiplist(sl);
    pthread_mutex_destroy(&sl->mutex);
    pthread_cond_destroy(&sl->cond);
    if (sl->flags & _SKIPLIST_FLAG_ALLOC) free(sl);
}

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

#include "libosns-basic-system-headers.h"

#include <math.h>

#include "skiplist.h"
#include "skiplist-utils.h"
#include "libosns-log.h"

static char *get_logname_default(struct list_element_s *l)
{
    return "";
}

static int compare_end(struct list_element_s *l, void *n)
{
    return 1;
}

int init_sl_skiplist(struct sl_skiplist_s *sl,
		    int (* compare) (struct list_element_s *l, void *b),
		    struct list_element_s *(* get_list_element) (void *lookupdata, struct sl_skiplist_s *sl),
		    char *(* get_logname)(struct list_element_s *l), void *ptr)
{
    unsigned int error=0;

    if ( ! sl) {

	error=EINVAL;

    } else if (! get_list_element || ! compare) {

	error=EINVAL;

    }

    if (error==0) {

	sl->ops.compare=compare;
	sl->ops.get_list_element=get_list_element;
	sl->ops.get_logname=(get_logname ? get_logname : get_logname_default);

	for (unsigned int i=0; i<=sl->maxlevel+1; i++) {

	    sl->dirnode.junction[i].n=&sl->dirnode;
	    sl->dirnode.junction[i].p=&sl->dirnode;

	}

	if (sl->dirnode.flags & _DIRNODE_FLAG_START) {

	    sl->dirnode.compare = compare_end;

	} else {

	    sl->dirnode.compare = compare;

	}

	return 0;

    }

    logoutput("init_sl_skiplist: error %i (%s)", error, strerror(error));
    return -1;

}

static unsigned char calc_nrlanes_from_size(unsigned int size)
{
    int tmp=(int) size;
    unsigned char nrlanes=0;

    if (tmp<=sizeof(struct sl_skiplist_s)) return 0;
    tmp-=sizeof(struct sl_skiplist_s);
    nrlanes=(tmp / sizeof(struct sl_junction_s));
    if (nrlanes <= 1) return 0;
    nrlanes-=1;

    return nrlanes;

}

struct sl_skiplist_s *create_sl_skiplist(struct sl_skiplist_s *sl, unsigned char prob, unsigned int size, unsigned char maxlanes)
{
    unsigned char nrlanes=0;

    logoutput_debug("create_sl_skiplist: prob %i size %i maxlanes %i", prob, size, maxlanes);

    prob=((prob==0) ? _SKIPLIST_PROB : prob);

    if (sl==NULL) {

	nrlanes=calc_nrlanes_from_size(size);
	if (nrlanes==0) return NULL;
	if (nrlanes != maxlanes) logoutput_warning("create_sl_skiplist: nrlanes %i differs from maxlanes %i", nrlanes, maxlanes);

	sl=malloc(size);
	if (sl==NULL) return NULL;
	memset(sl, 0, size);
	sl->flags=_SKIPLIST_FLAG_ALLOC;
	sl->size=size;

    } else {

	if (sl->size==0) sl->size=size;
	if (size>0 && size != sl->size) logoutput_warning("create_sl_skiplist: sl size %i differs from size %s", sl->size, size);

	if (sl->size>0) {

	    nrlanes=calc_nrlanes_from_size(sl->size);
	    if (nrlanes==0) return NULL;
	    if (nrlanes != maxlanes) logoutput_warning("create_sl_skiplist: nrlanes %i differs from maxlanes %i", nrlanes, maxlanes);

	}

    }


    sl->maxlevel=nrlanes - 1;
    sl->prob=prob;
    pthread_mutex_init(&sl->mutex, NULL);
    pthread_cond_init(&sl->cond, NULL);
    init_list_header(&sl->header, SIMPLE_LIST_TYPE_EMPTY, NULL);

    logoutput_debug("create_sl_skiplist: max nr lanes: %i", nrlanes);

    _init_sl_dirnode(sl, &sl->dirnode, size - sizeof(struct sl_skiplist_s), _DIRNODE_FLAG_START);
    sl->dirnode.level=0;

    logoutput_debug("create_sl_skiplist: ready");
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

    size=sizeof(struct sl_skiplist_s) + (maxlanes+1) * sizeof(struct sl_junction_s);
    logoutput("get_size_sl_skiplist: size %i = %i + (%i + 1) x %i" , size, sizeof(struct sl_skiplist_s), maxlanes, sizeof(struct sl_junction_s));
    return size;
}

void clear_sl_skiplist(struct sl_skiplist_s *sl)
{
    struct sl_dirnode_s *dirnode=sl->dirnode.junction[0].n;

    while (dirnode) {
	struct sl_dirnode_s *next=dirnode->junction[0].n; /* remind the next */

	if (dirnode->flags & _DIRNODE_FLAG_START) break;
	remove_sl_dirnode(sl, dirnode);
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

unsigned short get_default_sl_prob()
{
    return _SKIPLIST_PROB;
}

/* estimate the number of lanes given the number of elements */

unsigned char estimate_sl_lanes(uint64_t count, unsigned prob)
{
    return (unsigned char)(logl(count)/logl(prob));
}

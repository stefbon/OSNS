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
#include "skiplist-find.h"
#include "skiplist-utils.h"
#include "skiplist-lock.h"
#include "log.h"

static void move_lock_vector_path_down(struct sl_skiplist_s *sl, struct sl_vector_s *vector)
{
    unsigned short level=vector->level;
    struct sl_dirnode_s *dirnode=vector->path[level].dirnode;

    vector->path[level-1].dirnode=dirnode;
    vector->path[level-1].lock=vector->path[level].lock;
    vector->path[level-1].lockset=vector->path[level].lockset;
    vector->path[level].lock=0;
    vector->path[level].lockset=0;
    vector->level--;
}

static void move_lock_vector_down(struct sl_skiplist_s *sl, struct sl_vector_s *vector)
{
    int level=(int) vector->level;
    struct sl_dirnode_s *dirnode=vector->path[level].dirnode;

    if (level==0) return;

    vector->path[0].lock=vector->path[level].lock;
    vector->path[0].lockset=vector->path[level].lockset;
    vector->path[0].dirnode=dirnode;
    vector->path[level].lock=0;
    vector->path[level].lockset=0;

    level--;
    while (level>0) {

	vector->path[level].dirnode=dirnode;
	vector->path[level].lock=0;
	vector->path[level].lockset=0;
	level--;

    }

    vector->level=0;
}

void sl_find_generic(struct sl_skiplist_s *sl, unsigned char opcode, struct sl_lockops_s *lockops, void (* cb)(struct sl_skiplist_s *sl, struct sl_lockops_s *l, struct sl_vector_s *vector, struct sl_searchresult_s *result), struct sl_searchresult_s *result)
{
    struct sl_dirnode_s *dirnode=&sl->dirnode;
    struct sl_vector_s *vector=NULL;
    struct list_element_s *list=NULL;
    int diff=0;

    logoutput("sl_find_generic: level %i count: %i", sl->dirnode.level, sl->header.count);

    vector=create_vector_path(sl->dirnode.level+1, dirnode);

    if (vector==NULL) {

	result->flags |= SL_SEARCHRESULT_FLAG_ERROR;
	return;

    }

    /* create the locking startpoint at the left top of the skiplist */

    (* lockops->readlock_vector_path)(sl, vector, dirnode);
    vector->level--;
    logoutput_debug("sl_find_generic: A: vector level %i", vector->level);
    vector->path[vector->level].dirnode=dirnode;

    if (sl->header.count==0) {

	logoutput_debug("sl_find_generic: empty");
	result->flags |= SL_SEARCHRESULT_FLAG_EMPTY;
	move_lock_vector_down(sl, vector);
	goto out;

    }

    /* first check the extreme cases: before the first or the last

	TODO: do something with the information the name to lookup
	is closer to the last than the first */

    list=get_list_tail(&sl->header, 0);
    diff=sl->ops.compare(list, result->lookupdata);

    if (diff<0) {

	/* after the last: not found */
	logoutput_debug("sl_find_generic: after last");
	result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_LAST | SL_SEARCHRESULT_FLAG_AFTER);
	result->found=list;
	result->row=sl->header.count;
	move_lock_vector_down(sl, vector);
	goto out;

    } else if (diff==0) {

	// logoutput("sl_find_generic: last");
	result->found=list;
	result->flags |= (SL_SEARCHRESULT_FLAG_EXACT | SL_SEARCHRESULT_FLAG_LAST);
	result->row=sl->header.count;
	move_lock_vector_down(sl, vector);
	goto out;

    }

    list=get_list_head(&sl->header, 0);
    result->row=1;
    diff=sl->ops.compare(list, result->lookupdata);

    if (diff>0) {

	/* before the first: not found */
	logoutput_debug("sl_find_generic: before first");
	result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_FIRST | SL_SEARCHRESULT_FLAG_BEFORE);
	result->found=list;
	result->row=0;
	move_lock_vector_down(sl, vector);
	goto out;

    } else if (diff==0) {

	logoutput_debug("sl_find_generic: first");
	result->found=list;
	result->flags |= (SL_SEARCHRESULT_FLAG_EXACT | SL_SEARCHRESULT_FLAG_FIRST);
	move_lock_vector_down(sl, vector);
	goto out;

    }

    // vector->level--;
    // vector->path[vector->level].dirnode=dirnode;

    while (vector->level>=0) {
	struct sl_dirnode_s *next=NULL;

	// dirnode=vector->path[vector->level].dirnode;
	next=dirnode->junction[vector->level].n;
	diff=1;

	logoutput_debug("sl_find_generic: level %i step %i", vector->level, dirnode->junction[vector->level].step);

	diff=(* next->compare)(list, result->lookupdata);

	if (diff>0) {

	    /* diff>0: this next entry is too far, go one level down
		set another readlock on the same dirnode/junction */

	    if (vector->level==0) {

		logoutput_debug("sl_find_generic: C down level 0 -> goto linked list");
		break;

	    }

	    logoutput_debug("sl_find_generic: C down level (%i)", vector->level);
	    move_lock_vector_path_down(sl, vector);

	} else if (diff==0) {

	    /* exact match */

	    logoutput_debug("sl_find_generic: C exit (steps %i)", dirnode->junction[vector->level].step - 1);

	    result->found=list;
	    result->row+=dirnode->junction[vector->level].step - 1;
	    (* lockops->move_readlock_vector_path)(sl, vector, next);
	    result->flags |= (SL_SEARCHRESULT_FLAG_EXACT | SL_SEARCHRESULT_FLAG_DIRNODE);
	    move_lock_vector_down(sl, vector);
	    goto out;

	} else {

	    /* res<0: next_entry is smaller than name: skip
		move lock from previous, the level/lane stays the same  */

	    logoutput_debug("sl_find_generic: C skip (steps %i)", dirnode->junction[vector->level].step);

	    result->row+=dirnode->junction[vector->level].step;
	    (* lockops->move_readlock_vector_path)(sl, vector, next);
	    dirnode=next;
	    list=get_next_element(list);

	    if (list==NULL) {

		/* there is no next entry */

		result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_LAST | SL_SEARCHRESULT_FLAG_AFTER);
		result->found=next->list;
		move_lock_vector_down(sl, vector);
		goto out;

	    }

	    result->step=1;

	}

    }

    while (list && (result->ctr<=sl->header.count)) {

	diff=sl->ops.compare(list, result->lookupdata);
	result->ctr++;

	logoutput_debug("sl_find_generic: D %s diff: %i", sl->ops.get_logname(list), diff);

	if (diff<0) {

	    /* before name still */
	    list=get_next_element(list);
	    result->row++;
	    result->step++;

	    if (list==NULL) {

		result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_AFTER);
		break;

	    }

	} else if (diff==0) {

	    /* exact match */
	    result->found=list;
	    result->flags |= SL_SEARCHRESULT_FLAG_EXACT;
	    break;

	} else {

	    /* past name, no exact match */
	    result->found=list;
	    result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_BEFORE);
	    break;

	}

    }

    out:
    (* cb)(sl, lockops, vector, result);
}

static void correct_dirnodes_ignore(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *insert)
{
}

static void sl_find_cb(struct sl_skiplist_s *sl, struct sl_lockops_s *l, struct sl_vector_s *v, struct sl_searchresult_s *r)
{
    /* when finding an entry do nothing ... */
    (* l->remove_lock_vector)(sl, v, correct_dirnodes_ignore, NULL);
}

void sl_find(struct sl_skiplist_s *sl, struct sl_searchresult_s *result)
{
    struct sl_lockops_s *lockops=(result->flags & SL_SEARCHRESULT_FLAG_EXCLUSIVE) ? get_sl_lockops_nolock() : get_sl_lockops_default();
    sl_find_generic(sl, _SL_OPCODE_FIND, lockops, sl_find_cb, result);
}

/*
  2010, 2011, 2012, 2013 Stef Bon <stefbon@gmail.com>

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

#include <math.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "skiplist.h"
#include "skiplist-utils.h"
#include "skiplist-find.h"
#include "skiplist-insert.h"
#include "skiplist-lock.h"
#include "log.h"

/*

    determine the level of a new dirnode when an entry is added

    this function tests the number of dirnodes is sufficient when one entry is added

    return:

    level=-1: no dirnode required
    level>=0: a dirnode required with level, 
    it's possible this is +1 the current level, in that case an extra lane is required

*/

static int do_levelup(struct sl_skiplist_s *sl)
{
    int level=-1;
    unsigned hlp=0;

    // logoutput("do_levelup");

    pthread_mutex_lock(&sl->mutex);

    hlp=sl->header.count;

    if ( hlp > sl->prob ) {
	struct sl_dirnode_s *dirnode=&sl->dirnode;
	unsigned ctr=0;

	while (ctr<=dirnode->level) {

	    /* calculate the average steps between dirnodes */

	    hlp=hlp / (dirnode->junction[ctr].count + 1);
	    if (hlp > sl->prob) level=ctr;
	    ctr++;

	}

	if (level==dirnode->level) {

	    /* test an extra lane is required */

	    if ( hlp > sl->prob ) level++;

	}

    } else {

	level=0;

    }

    pthread_mutex_unlock(&sl->mutex);
    return level;
}

static void correct_dirnodes_insert(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *insert)
{
    struct sl_dirnode_s *new=insert->dirnode;
    struct sl_dirnode_s *prev=vector->path[level].dirnode;

    if (new && new->level>=level) {
	struct sl_dirnode_s *next=prev->junction[level].n;

	// logoutput("correct_dirnodes_insert: level %i new level %i prev flag %i next flags %i", level, new->level, prev->flags, next->flags);

	/* put new between prev and next on this level/lane */

	prev->junction[level].n=new;
	new->junction[level].p=prev;
	next->junction[level].p=new;
	new->junction[level].n=next;

	if (level>0) {
	    struct sl_dirnode_s *tmp=vector->path[level-1].dirnode;

	    if (prev != tmp) {

		/* skip dirnode: adjust the steps */

		insert->left += prev->junction[level-1].step;

	    }

	    if (next != tmp->junction[level-1].n) {
		struct sl_dirnode_s *tmp2=next->junction[level-1].p;

		/* skip dirnode: adjust the steps */

		insert->right += tmp2->junction[level-1].step;

	    }

	}

	prev->junction[level].step=insert->left;
	new->junction[level].step=insert->right;
	sl->dirnode.junction[level].count++;

	if (level==new->level) {

	    if (new->level > vector->maxlevel) {

		// logoutput("correct_dirnodes_insert: new level %i bigger than vector level %i", new->level, vector->maxlevel);

	    }

	}

    } else {

	// logoutput("correct_dirnodes_insert: level %i prev flag %i", level, prev->flags);

	/* adjust the lanes above the new dirnode
	    next and previous stay the same */

	prev->junction[level].step++;

    }

}

static void correct_dirnodes_ignore(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *insert)
{
}

static struct sl_dirnode_s *insert_sl_dirnode(struct sl_skiplist_s *sl, struct list_element_s *list, struct sl_move_dirnode_s *insert)
{
    struct sl_dirnode_s *new=NULL;
    int newlevel=do_levelup(sl);

    // logoutput("insert_sl_dirnode: newlevel %i left %i right %i", newlevel, insert->left, insert->right);

    if (newlevel>=0) {

	if (newlevel>sl->maxlevel) newlevel=sl->maxlevel;

	new=create_sl_dirnode(sl, newlevel);

	if (new) {

	    new->list=list;
	    if (newlevel>sl->dirnode.level) sl->dirnode.level=newlevel;
	    insert->dirnode=new;

	}

    }

    return new;

}

static void insert_sl_dirnode_left(struct sl_skiplist_s *sl, struct list_element_s *list, struct sl_move_dirnode_s *insert)
{
    struct sl_dirnode_s *new=insert_sl_dirnode(sl, list, insert);
    if (new) move_sl_dirnode_left(sl, insert);
}

static void insert_sl_dirnode_right(struct sl_skiplist_s *sl, struct list_element_s *list, struct sl_move_dirnode_s *insert)
{
    struct sl_dirnode_s *new=insert_sl_dirnode(sl, list, insert);
    if (new) move_sl_dirnode_right(sl, insert);
}

/*	insert an entry in the linked list and adapt the skiplist to that
	there are three situations:
	- entry is before the first dirnode
	- entry is after the last dirnode
	- every other case: entry is in between

	note: vector points to dirnode, which is the closest dirnode left to the entry
*/

static void sl_insert_cb(struct sl_skiplist_s *sl, struct sl_lockops_s *lockops, struct sl_vector_s *vector, struct sl_searchresult_s *result)
{

    logoutput_debug("sl_insert_cb");

    if ((result->flags & SL_SEARCHRESULT_FLAG_EXACT)==0 && (* lockops->upgrade_readlock_vector)(sl, vector, 0)==0) {
	struct list_element_s *list=(* sl->ops.get_list_element)(result->lookupdata, sl);
	struct sl_move_dirnode_s insert;
	struct sl_dirnode_s *prev=vector->path[0].dirnode;

	if (list==NULL) {

	    result->flags |= SL_SEARCHRESULT_FLAG_ERROR;
	    goto unlocknothing;

	}

	insert.flags=0;
	insert.dirnode=NULL;
	insert.left=0;
	insert.right=0;
	insert.step=0;
	insert.list=NULL;

	result->flags |= SL_SEARCHRESULT_FLAG_OK;

	if (result->flags & SL_SEARCHRESULT_FLAG_EMPTY) {

	    logoutput_debug("sl_insert_cb: add %s first result step %i dirnode step %i", sl->ops.get_logname(list), result->step, (prev) ? prev->junction[0].step : -1);

	    add_list_element_first(&sl->header, list);
	    sl->dirnode.junction[0].step=1; /* one element */
	    result->flags |= SL_SEARCHRESULT_FLAG_OK;
	    result->found=list;
	    goto unlock;

	} else if (result->flags & SL_SEARCHRESULT_FLAG_BEFORE) {

	    logoutput_debug("sl_insert_cb: add %s before %s result step %i dirnode step %i", sl->ops.get_logname(list), sl->ops.get_logname(result->found), result->step, (prev) ? prev->junction[0].step : -1);

	    add_list_element_before(&sl->header, result->found, list);
	    insert.left=result->step;
	    insert.right=prev->junction[0].step - result->step + 1;
	    result->flags |= SL_SEARCHRESULT_FLAG_OK;
	    result->found=list;

	} else if (result->flags & SL_SEARCHRESULT_FLAG_AFTER) {

	    logoutput_debug("sl_insert_cb: add %s after %s result step %i dirnode step %i", sl->ops.get_logname(list), sl->ops.get_logname(result->found), result->step, (prev) ? prev->junction[0].step : -1);

	    add_list_element_after(&sl->header, result->found, list);
	    insert.left=result->step + 1;
	    insert.right=prev->junction[0].step - result->step;
	    result->flags |= SL_SEARCHRESULT_FLAG_OK;
	    result->found=list;

	} else {

	    logoutput_warning("sl_insert_cb: no add_list_element flags %i", result->flags);

	}

	if (insert.left>=sl->prob && insert.right>=sl->prob) {

	    /* put an dirnode on the list element */

	    logoutput_debug("sl_insert_cb: C left %i right %i", insert.left, insert.right);
	    insert_sl_dirnode(sl, list, &insert);

	} else if ((insert.left + insert.right + 1 >= 2 * sl->prob) || insert.left > sl->prob + 1 || insert.left > sl->prob + 1) {

	    if (insert.left > insert.right) {

		/* left of the new entry is more space */

		logoutput_debug("sl_insert_cb: D left %i right %i", insert.left, insert.right);
		insert_sl_dirnode_left(sl, list, &insert);

	    } else if (insert.right > insert.left) {

		/* right of the new entry is more space */

		logoutput_debug("sl_insert_cb: E left %i right %i", insert.left, insert.right);
		insert_sl_dirnode_right(sl, list, &insert);

	    }

	} else {

	    logoutput_debug("sl_insert_cb: F left %i right %i", insert.left, insert.right);

	}

	unlock:
	(* lockops->remove_lock_vector)(sl, vector, correct_dirnodes_insert, &insert);
	return;

    }

    unlocknothing:
    (* lockops->remove_lock_vector)(sl, vector, correct_dirnodes_ignore, NULL);

}

void sl_insert(struct sl_skiplist_s *sl, struct sl_searchresult_s *result)
{
    struct sl_lockops_s *lockops=(result->flags & SL_SEARCHRESULT_FLAG_EXCLUSIVE) ? get_sl_lockops_nolock() : get_sl_lockops_default();
    sl_find_generic(sl, _SL_OPCODE_INSERT, lockops, sl_insert_cb, result);
}

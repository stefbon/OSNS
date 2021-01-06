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

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "skiplist.h"
#include "skiplist-find.h"
#include "skiplist-delete.h"
#include "skiplist-utils.h"
#include "skiplist-lock.h"
#define LOGGING
#include "log.h"

static void correct_sl_dirnodes_remove(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *move)
{
    struct sl_dirnode_s *old=move->dirnode;

    if (old && old->level >= level) {
	struct sl_dirnode_s *prev=old->junction[level].p;
	struct sl_dirnode_s *next=old->junction[level].n;

	prev->junction[level].n=next;
	prev->junction[level].step += old->junction[level].step - 1;

	sl->dirnode.junction[level].count--;

	if (level==old->level) {

	    free(old);
	    move->dirnode=NULL;

	}

    } else {
	struct sl_dirnode_s *dirnode=vector->path[level].dirnode;

	dirnode->junction[level].step--;

    }

}

static void correct_sl_dirnodes_move(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *move)
{
    struct sl_dirnode_s *dirnode=move->dirnode;

    if (dirnode && dirnode->level>=level) {
	struct sl_dirnode_s *prev=dirnode->junction[level].p;

	prev->junction[level].step += move->step;
	dirnode->junction[level].step -= move->step;

	if (move->flags & _SL_MOVE_FLAG_DIRNODERIGHT) {

	    prev->junction[level].step--;

	} else {

	    dirnode->junction[level].step--;

	}

    } else {
	struct sl_dirnode_s *dirnode=vector->path[level].dirnode;

	dirnode->junction[level].step--;

    }

}

static void correct_sl_dirnodes_ignore(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *move)
{
}

static void sl_delete_cb(struct sl_skiplist_s *sl, struct sl_lockops_s *lockops, struct sl_vector_s *vector, struct sl_searchresult_s *result)
{
    unsigned char move=(result->flags & SL_SEARCHRESULT_FLAG_DIRNODE) ? 1 : 0;
    struct sl_dirnode_s *dirnode=vector->path[0].dirnode;
    void (* cb)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned int level, struct sl_move_dirnode_s *move);

    logoutput("sl_delete_cb");
    cb=correct_sl_dirnodes_ignore;

    /* upgrade to write lock */

    if ((result->flags & SL_SEARCHRESULT_FLAG_EXACT) && (*lockops->upgrade_readlock_vector)(sl, vector, move)==0) {
	struct list_element_s *list=result->found;
	struct sl_move_dirnode_s move;

	logoutput("sl_delete_cb: A");

	move.flags=0;
	move.dirnode=NULL;
	move.left=0;
	move.right=0;
	move.step=0;
	move.list=list;
	cb=correct_sl_dirnodes_move;

	if (result->flags & SL_SEARCHRESULT_FLAG_DIRNODE) {
	    struct sl_dirnode_s *prev=dirnode->junction[0].p;

	    if (prev->junction[0].step + dirnode->junction[0].step + 1 < 2 * sl->prob) {

		/* remove the dirnode */

		move.dirnode=dirnode;
		move.left=prev->junction[0].step;
		move.right=dirnode->junction[0].step;
		move.flags |= _SL_MOVE_FLAG_REMOVE;
		cb=correct_sl_dirnodes_remove;

	    } else if (prev->junction[0].step > dirnode->junction[0].step + 1) {

		/* move to the left */

		move.dirnode=dirnode;
		move.left=prev->junction[0].step;
		move.right=dirnode->junction[0].step;
		move.flags |= _SL_MOVE_FLAG_MOVELEFT;
		move_sl_dirnode_left(sl, &move);

	    } else if (dirnode->junction[0].step > prev->junction[0].step + 1) {

		/* move to the right */

		move.dirnode=dirnode;
		move.left=prev->junction[0].step;
		move.right=dirnode->junction[0].step;
		move.flags |= _SL_MOVE_FLAG_MOVELEFT;
		move_sl_dirnode_right(sl, &move);

	    }

	} else if (dirnode->junction[0].step < sl->prob - 1) {
	    struct sl_dirnode_s *next=dirnode->junction[0].n;

	    if ((dirnode->flags & _DIRNODE_FLAG_START)==0 && (next->flags & _DIRNODE_FLAG_START)==0) {
		struct sl_dirnode_s *prev=dirnode->junction[0].p;

		/* where is most space: before dirnode of after next ?*/

		if (dirnode->junction[0].step > next->junction[0].step) {

		    /* move the next to the left */

		    move.dirnode=next;
		    move.left=dirnode->junction[0].step;
		    move.right=next->junction[0].step;
		    move.flags |= _SL_MOVE_FLAG_DIRNODERIGHT | _SL_MOVE_FLAG_MOVELEFT;
		    move_sl_dirnode_left(sl, &move);

		} else if (prev->junction[0].step > next->junction[0].step) {

		    /* move dirnode to the left */

		    move.dirnode=dirnode;
		    move.left=prev->junction[0].step;
		    move.right=dirnode->junction[0].step;
		    move.flags |= _SL_MOVE_FLAG_MOVELEFT;
		    move_sl_dirnode_left(sl, &move);

		} else if (dirnode->junction[0].step < next->junction[0].step) {

		    /* move the next to the right */

		    move.dirnode=next;
		    move.left=dirnode->junction[0].step;
		    move.right=next->junction[0].step;
		    move.flags |= _SL_MOVE_FLAG_DIRNODERIGHT | _SL_MOVE_FLAG_MOVERIGHT;
		    move_sl_dirnode_right(sl, &move);

		}

	    } else if ((dirnode->flags & _DIRNODE_FLAG_START) && (next->flags & _DIRNODE_FLAG_START)==0) {

		if (dirnode->junction[0].step < next->junction[0].step) {

		    /* create space by moving next to the right */

		    move.dirnode=next;
		    move.left=dirnode->junction[0].step;
		    move.right=next->junction[0].step;
		    move.flags |= _SL_MOVE_FLAG_DIRNODERIGHT | _SL_MOVE_FLAG_MOVERIGHT;
		    move_sl_dirnode_right(sl, &move);

		} else if (dirnode->junction[0].step > next->junction[0].step) {

		    /* move the next to the left */

		    move.dirnode=next;
		    move.left=dirnode->junction[0].step;
		    move.right=next->junction[0].step;
		    move.flags |= _SL_MOVE_FLAG_DIRNODERIGHT | _SL_MOVE_FLAG_MOVELEFT;
		    move_sl_dirnode_left(sl, &move);

		}

	    } else if ((dirnode->flags & _DIRNODE_FLAG_START)==0 && (next->flags & _DIRNODE_FLAG_START)) {
		struct sl_dirnode_s *prev=dirnode->junction[0].p;

		if (prev->junction[0].step > dirnode->junction[0].step) {

		    /* create space by moving dirnode to the left */

		    move.dirnode=dirnode;
		    move.left=prev->junction[0].step;
		    move.right=dirnode->junction[0].step;
		    move.flags |= _SL_MOVE_FLAG_MOVERIGHT;
		    move_sl_dirnode_left(sl, &move);

		}

	    }

	}

	remove_list_element(list);
	result->flags |= SL_SEARCHRESULT_FLAG_OK;

	logoutput("sl_delete_cb: unlock");
	(* lockops->remove_lock_vector)(sl, vector, cb, &move);
	return;

    }

    if ((result->flags & SL_SEARCHRESULT_FLAG_EXACT)==0) {

	logoutput("sl_delete_cb: not able to update readlock");

    } else {

	result->flags |= SL_SEARCHRESULT_FLAG_ERROR;

    }

    (* lockops->remove_lock_vector)(sl, vector, cb, NULL);

}

void sl_delete(struct sl_skiplist_s *sl, struct sl_searchresult_s *result)
{
    struct sl_lockops_s *lockops=(result->flags & SL_SEARCHRESULT_FLAG_EXCLUSIVE) ? get_sl_lockops_nolock() : get_sl_lockops_default();
    sl_find_generic(sl, _SL_OPCODE_DELETE, lockops, sl_delete_cb, result);
}

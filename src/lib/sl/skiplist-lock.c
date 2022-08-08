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

#include "skiplist.h"
#include "libosns-log.h"

/* remove the read locks set by the vector */

static void remove_lock_vector(struct sl_skiplist_s *sl, struct sl_vector_s *vector, void (* cb)(struct sl_skiplist_s *sl, struct sl_vector_s *v, unsigned int l, struct sl_move_dirnode_s *m), struct sl_move_dirnode_s *move)
{

    // logoutput("remove_lock_vector: sl level %i vector level %i vector maxlevel %i", sl->dirnode.level, vector->level, vector->maxlevel);

    pthread_mutex_lock(&sl->mutex);

    for (unsigned int i=vector->level; i<=vector->maxlevel; i++) {
	struct sl_dirnode_s *dirnode=vector->path[i].dirnode;

	(* cb)(sl, vector, i, move);

	if (vector->path[i].lock) {

	    dirnode->lock &= ~vector->path[i].lockset;
	    dirnode->lockers--;

	}

	// logoutput("remove_lock_vector: i %i lock %i lockset %i dlock %i dlockers %i", i, vector->path[i].lock, vector->path[i].lockset, dirnode->lock, dirnode->lockers);

	// vector->path[i].dirnode=NULL;
	vector->path[i].lock=0;
	vector->path[i].lockset=0;

    }

    pthread_cond_broadcast(&sl->cond);
    pthread_mutex_unlock(&sl->mutex);

}

static void remove_lock_vector_nolock(struct sl_skiplist_s *sl, struct sl_vector_s *vector, void (* cb)(struct sl_skiplist_s *sl, struct sl_vector_s *v, unsigned int l, struct sl_move_dirnode_s *m), struct sl_move_dirnode_s *move)
{

}

/* set a readlock on path element */

static void readlock_vector_path(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode)
{
    unsigned char block = _DIRNODE_LOCK_WRITE | _DIRNODE_LOCK_PREWRITE;
    unsigned short level=vector->level;

    pthread_mutex_lock(&sl->mutex);

    while (dirnode->lock & block) pthread_cond_wait(&sl->cond, &sl->mutex);

    vector->path[level].dirnode=dirnode;
    vector->path[level].lock=1;
    vector->path[level].lockset=_DIRNODE_LOCK_READ;
    dirnode->lock &= _DIRNODE_LOCK_READ;
    dirnode->lockers++;

    pthread_mutex_unlock(&sl->mutex);
}

static void readlock_vector_path_nolock(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode)
{
    vector->path[vector->level].dirnode=dirnode;
}

/* remove a readlock on a path element */

static void remove_readlock_vector_path(struct sl_skiplist_s *sl, struct sl_vector_s *vector)
{
    unsigned short level=vector->level;

    if (vector->path[level].lock) {
	struct sl_dirnode_s *dirnode=vector->path[level].dirnode;

	pthread_mutex_lock(&sl->mutex);

	dirnode->lockers--;

	if (dirnode->lockers==0) {

	    dirnode->lock &= ~_DIRNODE_LOCK_READ;
	    pthread_cond_broadcast(&sl->cond);

	}

	vector->path[level].lock=0;
	vector->path[level].lockset=0;

	pthread_mutex_unlock(&sl->mutex);

    }

}

static void remove_readlock_vector_path_nolock(struct sl_skiplist_s *sl, struct sl_vector_s *vector)
{
}

/* move a readlock to the other dirnode (mostly the next or the prev)
    usefull when skipping a dirnode (from left to write) or moving the left when deleting a dirnode */

static void move_readlock_vector_path(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode)
{
    unsigned char block = _DIRNODE_LOCK_WRITE | _DIRNODE_LOCK_PREWRITE;
    unsigned short level=vector->level;

    if (dirnode==vector->path[level].dirnode) return;

    pthread_mutex_lock(&sl->mutex);

    if (vector->path[level].lock) {
	struct sl_dirnode_s *tmp=vector->path[level].dirnode;

	tmp->lockers--;

	if (tmp->lockers==0) {

	    tmp->lock &= ~_DIRNODE_LOCK_READ;
	    pthread_cond_broadcast(&sl->cond);

	}

	vector->path[level].lock=0;

    }

    while (dirnode->lock & block) pthread_cond_wait(&sl->cond, &sl->mutex);

    vector->path[level].dirnode=dirnode;
    vector->path[level].lock=1;
    vector->path[level].lockset=_DIRNODE_LOCK_READ;
    dirnode->lock &= _DIRNODE_LOCK_READ;
    dirnode->lockers++;

    pthread_mutex_unlock(&sl->mutex);

}

static void move_readlock_vector_path_nolock(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode)
{
    vector->path[vector->level].dirnode=dirnode;
}


/* upgrade a readlock on a dirnode to a write/exclusive lock
    used when a dirnode is inserted or deleted */

static int upgrade_readlock_vector_path(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned short level)
{
    struct sl_dirnode_s *dirnode=vector->path[level].dirnode;
    unsigned char block = _DIRNODE_LOCK_WRITE | _DIRNODE_LOCK_PREWRITE | _DIRNODE_LOCK_READ;
    unsigned char lock=0;

    // logoutput("upgrade_readlock_vector_path: lockers %i", dirnode->lockers);

    /* wait for there will be only one reader here (==self) and upgrade that to a writer
	dirnode->readers -> 1 -> 0
	dirnode->lock READ -> WRITE
	so here a wait for other readers to dissappear
	*/

    pthread_mutex_lock(&sl->mutex);

    if (dirnode->lock & _DIRNODE_LOCK_PREWRITE) {

	/* already another registered to get write lock: exit to prevent deadlock */
	dirnode->lockers--;
	if (dirnode->lockers==0) dirnode->lock &= ~_DIRNODE_LOCK_READ;
	vector->path[level].lockset &= ~_DIRNODE_LOCK_READ;
	vector->path[level].lock=0;
	pthread_mutex_unlock(&sl->mutex);
	return -1;

    }

    dirnode->lock |= _DIRNODE_LOCK_PREWRITE;
    lock|=_DIRNODE_LOCK_PREWRITE;
    block-=_DIRNODE_LOCK_PREWRITE;

    /* there may be more readers */

    while (dirnode->lockers>1) pthread_cond_wait(&sl->cond, &sl->mutex);

    dirnode->lock &= _DIRNODE_LOCK_WRITE;
    dirnode->lock &= ~lock;
    dirnode->lockers++;
    vector->path[level].lockset |= _DIRNODE_LOCK_WRITE;


    dirnode->lock &= ~_DIRNODE_LOCK_READ;
    dirnode->lockers--;
    vector->path[level].lockset &= ~_DIRNODE_LOCK_READ;
    pthread_mutex_unlock(&sl->mutex);
    return 0;

}

static int upgrade_readlock_vector(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned char move)
{
    unsigned short level=vector->level;
    struct sl_dirnode_s *dirnode=vector->path[0].dirnode;
    int result=0;

    while (level<=vector->maxlevel) {

	/* when deleting a dirnode for example move the lock to the previous
	    assume the caller checked the boundaries: dirnode is not the starting dirnode */

	if (move && vector->path[level].dirnode==dirnode) {
	    struct sl_dirnode_s *prev=dirnode->junction[level].p;

	    move_readlock_vector_path(sl, vector, prev);

	}

	if (vector->path[level].lock) {

	    if (upgrade_readlock_vector_path(sl, vector, level)==-1) {

		/* not able to upgrade a readlock on a junction to a writelock, so return
		all the blocks set (read and write, also the upgraded) will be removed by the generic function
		remove_lock_vector */

		result=-1;
		break;

	    }

	}

	level++;

    }

    return result;
}

static int upgrade_readlock_vector_nolock(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned char move)
{
    return 0;
}

static struct sl_lockops_s sl_default_lockops = {
	.remove_lock_vector				= remove_lock_vector,
	.readlock_vector_path				= readlock_vector_path,
	.remove_readlock_vector_path			= remove_readlock_vector_path,
	.move_readlock_vector_path			= move_readlock_vector_path,
	.upgrade_readlock_vector			= upgrade_readlock_vector,
};

static struct sl_lockops_s sl_nolock_lockops = {
	.remove_lock_vector				= remove_lock_vector_nolock,
	.readlock_vector_path				= readlock_vector_path_nolock,
	.remove_readlock_vector_path			= remove_readlock_vector_path_nolock,
	.move_readlock_vector_path			= move_readlock_vector_path_nolock,
	.upgrade_readlock_vector			= upgrade_readlock_vector_nolock,
};

struct sl_lockops_s *get_sl_lockops_default()
{
    return &sl_default_lockops;
}

struct sl_lockops_s *get_sl_lockops_nolock()
{
    return &sl_nolock_lockops;
}

/*

  2018 Stef Bon <stefbon@gmail.com>

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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <pthread.h>
#include "simple-list.h"
#include "simple-locking.h"
#undef LOGGING
#include "log.h"

int init_simple_locking(struct simple_locking_s *locking)
{
    locking->flags=0;
    pthread_mutex_init(&locking->mutex, NULL);
    pthread_cond_init(&locking->cond, NULL);
    init_list_header(&locking->readlocks, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&locking->writelocks, SIMPLE_LIST_TYPE_EMPTY, NULL);
    locking->readers=0;
    locking->writers=0;
    return 0;
}

void clear_simple_locking(struct simple_locking_s *locking)
{
    struct list_element_s *list=NULL;

    pthread_mutex_destroy(&locking->mutex);
    pthread_cond_destroy(&locking->cond);

    list=get_list_head(&locking->readlocks, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct simple_lock_s *lock=(struct simple_lock_s *) (((char *) list) - offsetof(struct simple_lock_s, list));

	free(lock);
	list=get_list_head(&locking->readlocks, SIMPLE_LIST_FLAG_REMOVE);

    }

    list=get_list_head(&locking->writelocks, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct simple_lock_s *lock=(struct simple_lock_s *) (((char *) list) - offsetof(struct simple_lock_s, list));

	free(lock);
	list=get_list_head(&locking->writelocks, SIMPLE_LIST_FLAG_REMOVE);

    }

}

static int match_simple_lock(struct list_element_s *element, void *ptr)
{
    pthread_t *thread=(pthread_t *) ptr;
    struct simple_lock_s *lock=(struct simple_lock_s *) (((char *) element) - offsetof(struct simple_lock_s, list));

    if (lock->thread==*thread) return 0;
    return -1;
}

static int _simple_nonelock(struct simple_lock_s *lock)
{
    return 0;
}

static int _simple_noneunlock(struct simple_lock_s *rlock)
{
    return 0;
}

static int _simple_upgrade_nonelock(struct simple_lock_s *rlock)
{
    return 0;
}

static int _simple_downgrade_nonelock(struct simple_lock_s *rlock)
{
    return 0;
}

static int _simple_prenonelock(struct simple_lock_s *rlock)
{
    return 0;
}

/* simple read lock */

static int _simple_readlock(struct simple_lock_s *rlock)
{
    struct simple_locking_s *locking=rlock->locking;

    rlock->thread=pthread_self();
    pthread_mutex_lock(&locking->mutex);

    /* if already part of list prevent doing that again */
    if ((rlock->flags & SIMPLE_LOCK_FLAG_LIST)==0) {

	while (locking->writers>0) pthread_cond_wait(&locking->cond, &locking->mutex);
	add_list_element_last(&locking->readlocks, &rlock->list);
	rlock->flags|=SIMPLE_LOCK_FLAG_LIST;
	locking->readers++;

    }

    rlock->flags|=SIMPLE_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(&locking->mutex);
    return 0;

}

/* unlock a read lock */

static int _simple_readunlock(struct simple_lock_s *rlock)
{
    struct simple_locking_s *locking=rlock->locking;

    pthread_mutex_lock(&locking->mutex);

    /* only when part of list remove it from list */
    if (rlock->flags & SIMPLE_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	rlock->flags -= SIMPLE_LOCK_FLAG_LIST;
	locking->readers--;
	pthread_cond_broadcast(&locking->cond);

    }

    if (rlock->flags & SIMPLE_LOCK_FLAG_EFFECTIVE) rlock->flags -= SIMPLE_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(&locking->mutex);
    return 0;
}

/* full write lock */

static int _simple_writelock(struct simple_lock_s *wlock)
{
    struct simple_locking_s *locking=wlock->locking;

    wlock->thread=pthread_self();
    pthread_mutex_lock(&locking->mutex);

    if ((wlock->flags & SIMPLE_LOCK_FLAG_LIST)==0) {

	add_list_element_last(&locking->writelocks, &wlock->list);
	locking->writers++;
	wlock->flags |= SIMPLE_LOCK_FLAG_LIST;

    }

    if ((wlock->flags & SIMPLE_LOCK_FLAG_EFFECTIVE)==0) {

	/* only get the write lock when there are no readers and this wlock is the first */

	while ((locking->readers>0) && (list_element_is_first(&wlock->list)==-1)) pthread_cond_wait(&locking->cond, &locking->mutex);
	wlock->flags |= SIMPLE_LOCK_FLAG_EFFECTIVE;

    }

    pthread_mutex_unlock(&locking->mutex);
    return 0;

}

/* unlock a write lock */

static int _simple_writeunlock(struct simple_lock_s *wlock)
{
    struct simple_locking_s *locking=wlock->locking;

    pthread_mutex_lock(&locking->mutex);

    if (wlock->flags & SIMPLE_LOCK_FLAG_LIST) {

	remove_list_element(&wlock->list);
	locking->writers--;
	wlock->flags -= SIMPLE_LOCK_FLAG_LIST;

    }

    if (wlock->flags & SIMPLE_LOCK_FLAG_UPGRADED) {

	if (locking->flags & SIMPLE_LOCKING_FLAG_UPGRADE) locking->flags-=SIMPLE_LOCKING_FLAG_UPGRADE;
	wlock->flags-=SIMPLE_LOCK_FLAG_UPGRADED;

    }

    pthread_cond_broadcast(&locking->cond);
    pthread_mutex_unlock(&locking->mutex);

    if (wlock->flags & SIMPLE_LOCK_FLAG_EFFECTIVE) wlock->flags -= SIMPLE_LOCK_FLAG_EFFECTIVE;

    return 0;
}

/* upgrade a write lock (does nothing since there is nothing "above" a write lock */

static int _simple_upgrade_writelock(struct simple_lock_s *wlock)
{
    return 0;
}

/* queue the write lock to get later a write lock
    queueing it will prevent any readers in the meantime
    when using this lock it may have become the first */

static int _simple_prewritelock(struct simple_lock_s *wlock)
{
    struct simple_locking_s *locking=wlock->locking;

    wlock->thread=pthread_self();

    if ((wlock->flags & SIMPLE_LOCK_FLAG_LIST)==0) {

	pthread_mutex_lock(&locking->mutex);
	add_list_element_last(&locking->writelocks, &wlock->list);
	locking->writers++;
	pthread_mutex_unlock(&locking->mutex);

	wlock->flags |= SIMPLE_LOCK_FLAG_LIST;

    }

    return 0;

}

/* upgrade a readlock to a full write lock */

static int _simple_upgrade_readlock(struct simple_lock_s *rlock)
{
    struct simple_locking_s *locking=rlock->locking;

    if (rlock->type != SIMPLE_LOCK_TYPE_READ) return 0;

    pthread_mutex_lock(&locking->mutex);

    /* prevent two readers to upgrade at the same time */

    if (locking->flags & SIMPLE_LOCKING_FLAG_UPGRADE) {

	/* this readlock has to unlock hereafter, otherwise the upgrading lock will wait for this lock */
	pthread_mutex_unlock(&locking->mutex);
	return -1;

    }

    locking->flags|=SIMPLE_LOCKING_FLAG_UPGRADE;

    if (rlock->flags & SIMPLE_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	locking->readers--;

    }

    /* add as first in writers list, otherwise a deadlock is possible */
    add_list_element_first(&locking->writelocks, &rlock->list);
    rlock->flags |= SIMPLE_LOCK_FLAG_LIST;
    locking->writers++;

    rlock->type=SIMPLE_LOCK_TYPE_WRITE;
    rlock->lock=_simple_writelock;
    rlock->unlock=_simple_writeunlock;
    rlock->upgrade=_simple_upgrade_writelock;
    rlock->downgrade=
    rlock->prelock=_simple_prewritelock;
    rlock->flags |= SIMPLE_LOCK_FLAG_UPGRADED;

    pthread_cond_broadcast(&locking->cond);

    /* wait for no readers anymore (this writer is already the first) */
    while (locking->readers>0) pthread_cond_wait(&locking->cond, &locking->mutex);
    rlock->flags |= SIMPLE_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(&locking->mutex);

    return 0;

}

static int _simple_prereadlock(struct simple_lock_s *rlock);

static int _simple_downgrade_readlock(struct simple_lock_s *rlock)
{
    struct simple_locking_s *locking=rlock->locking;

    if ((rlock->flags & SIMPLE_LOCK_FLAG_UPGRADED)==0 || (rlock->type != SIMPLE_LOCK_TYPE_WRITE)) return 0;

    pthread_mutex_lock(&locking->mutex);

    locking->flags &= ~SIMPLE_LOCKING_FLAG_UPGRADE;

    if (rlock->flags & SIMPLE_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	locking->writers--;

    }

    /* add as first in writers list, otherwise a deadlock is possible */
    add_list_element_first(&locking->readlocks, &rlock->list);
    rlock->flags |= SIMPLE_LOCK_FLAG_LIST;
    locking->readers++;

    rlock->type=SIMPLE_LOCK_TYPE_READ;
    rlock->lock=_simple_readlock;
    rlock->unlock=_simple_readunlock;
    rlock->upgrade=_simple_upgrade_readlock;
    rlock->prelock=_simple_prereadlock;
    rlock->flags &= ~SIMPLE_LOCK_FLAG_UPGRADED;

    pthread_cond_broadcast(&locking->cond);

    rlock->flags |= SIMPLE_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(&locking->mutex);
    return 0;

}

/* turn a read lock into a pre write lock */

static int _simple_prereadlock(struct simple_lock_s *rlock)
{
    struct simple_locking_s *locking=rlock->locking;

    pthread_mutex_lock(&locking->mutex);

    if (rlock->flags & SIMPLE_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	locking->readers--;

    }

    add_list_element_first(&locking->writelocks, &rlock->list);
    locking->writers++;
    rlock->type=SIMPLE_LOCK_TYPE_WRITE;
    rlock->lock=_simple_writelock;
    rlock->unlock=_simple_writeunlock;
    rlock->upgrade=_simple_upgrade_writelock;
    rlock->prelock=_simple_prewritelock;

    pthread_mutex_unlock(&locking->mutex);
    return 0;
}
void init_simple_nonelock(struct simple_locking_s *locking, struct simple_lock_s *lock)
{
    lock->type=SIMPLE_LOCK_TYPE_NONE;
    lock->thread=0;
    init_list_element(&lock->list, NULL);
    lock->flags=0;
    lock->locking=locking;
    lock->lock=_simple_nonelock;
    lock->unlock=_simple_noneunlock;
    lock->upgrade=_simple_upgrade_nonelock;
    lock->prelock=_simple_prenonelock;
}
void init_simple_readlock(struct simple_locking_s *locking, struct simple_lock_s *rlock)
{
    rlock->type=SIMPLE_LOCK_TYPE_READ;
    rlock->thread=0;
    init_list_element(&rlock->list, NULL);
    rlock->flags=0;
    rlock->locking=locking;
    rlock->lock=_simple_readlock;
    rlock->unlock=_simple_readunlock;
    rlock->upgrade=_simple_upgrade_readlock;
    rlock->downgrade=_simple_downgrade_readlock;
    rlock->prelock=_simple_prereadlock;
}
void init_simple_writelock(struct simple_locking_s *locking, struct simple_lock_s *wlock)
{
    wlock->type=SIMPLE_LOCK_TYPE_WRITE;
    wlock->thread=0;
    init_list_element(&wlock->list, NULL);
    wlock->flags=0;
    wlock->locking=locking;
    wlock->lock=_simple_writelock;
    wlock->unlock=_simple_writeunlock;
    wlock->upgrade=_simple_upgrade_writelock;
    wlock->downgrade=_simple_upgrade_writelock;
    wlock->prelock=_simple_prewritelock;
}

int simple_lock(struct simple_lock_s *lock)
{
    return (* lock->lock)(lock);
}

int simple_unlock(struct simple_lock_s *lock)
{
    return (* lock->unlock)(lock);
}

int simple_prelock(struct simple_lock_s *lock)
{
    return (* lock->prelock)(lock);
}

int simple_upgradelock(struct simple_lock_s *lock)
{
    return (* lock->upgrade)(lock);
}

int simple_downgradelock(struct simple_lock_s *lock)
{
    return (* lock->downgrade)(lock);
}

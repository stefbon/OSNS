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

#include "libosns-basic-system-headers.h"

#include "libosns-list.h"
#include "libosns-log.h"

#include "locking.h"


static pthread_mutex_t global_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t global_cond=PTHREAD_COND_INITIALIZER;

int init_osns_locking(struct osns_locking_s *locking, unsigned int flags)
{

    if (flags && (flags & ~OSNS_LOCKING_FLAG_ALLOC_LOCK)) {

	logoutput_warning("init_osns_locking: error, parameters not valid (flags=%i)", flags);
	return -1;

    }

    locking->flags=0;

    if (flags & OSNS_LOCKING_FLAG_ALLOC_LOCK) {

	locking->mutex=malloc(sizeof(pthread_mutex_t));
	locking->cond=malloc(sizeof(pthread_cond_t));

	if (locking->mutex==NULL || locking->cond==NULL) {

	    free(locking->cond);
	    free(locking->mutex);
	    return -1;

	}

	pthread_mutex_init(locking->mutex, NULL);
	pthread_cond_init(locking->cond, NULL);

	locking->flags |= (OSNS_LOCKING_FLAG_ALLOC_MUTEX | OSNS_LOCKING_FLAG_ALLOC_COND);

    } else {

	locking->mutex=&global_mutex;
	locking->cond=&global_cond;

    }

    init_list_header(&locking->readlocks, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&locking->writelocks, SIMPLE_LIST_TYPE_EMPTY, NULL);
    locking->readers=0;
    locking->writers=0;
    return 0;
}

void clear_osns_locking(struct osns_locking_s *locking)
{
    struct list_element_s *list=NULL;

    if (locking->flags & OSNS_LOCKING_FLAG_ALLOC_MUTEX) {

	pthread_mutex_destroy(locking->mutex);
	free((char *) locking->mutex);
	locking->flags &= ~OSNS_LOCKING_FLAG_ALLOC_MUTEX;

    }

    locking->mutex=NULL;

    if (locking->flags & OSNS_LOCKING_FLAG_ALLOC_COND) {

	pthread_cond_destroy(locking->cond);
	free((char *) locking->cond);
	locking->flags &= ~OSNS_LOCKING_FLAG_ALLOC_COND;

    }

    locking->cond=NULL;

    list=get_list_head(&locking->readlocks, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct osns_lock_s *lock=(struct osns_lock_s *) (((char *) list) - offsetof(struct osns_lock_s, list));

	free(lock);
	list=get_list_head(&locking->readlocks, SIMPLE_LIST_FLAG_REMOVE);

    }

    list=get_list_head(&locking->writelocks, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct osns_lock_s *lock=(struct osns_lock_s *) (((char *) list) - offsetof(struct osns_lock_s, list));

	free(lock);
	list=get_list_head(&locking->writelocks, SIMPLE_LIST_FLAG_REMOVE);

    }

}

static int match_osns_lock(struct list_element_s *element, void *ptr)
{
    pthread_t *thread=(pthread_t *) ptr;
    struct osns_lock_s *lock=(struct osns_lock_s *) (((char *) element) - offsetof(struct osns_lock_s, list));

    if (lock->thread==*thread) return 0;
    return -1;
}

static int _osns_nonelock(struct osns_lock_s *lock)
{
    return 0;
}

static int _osns_noneunlock(struct osns_lock_s *rlock)
{
    return 0;
}

static int _osns_upgrade_nonelock(struct osns_lock_s *rlock)
{
    return 0;
}

static int _osns_downgrade_nonelock(struct osns_lock_s *rlock)
{
    return 0;
}

static int _osns_prenonelock(struct osns_lock_s *rlock)
{
    return 0;
}

/* simple read lock */

static int _osns_readlock(struct osns_lock_s *rlock)
{
    struct osns_locking_s *locking=rlock->locking;

    rlock->thread=pthread_self();
    pthread_mutex_lock(locking->mutex);

    /* if already part of list prevent doing that again */

    if ((rlock->flags & OSNS_LOCK_FLAG_LIST)==0) {

	while (locking->writers>0) pthread_cond_wait(locking->cond, locking->mutex);
	add_list_element_last(&locking->readlocks, &rlock->list);
	rlock->flags |= OSNS_LOCK_FLAG_LIST;
	locking->readers++;

    }

    rlock->flags|=OSNS_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(locking->mutex);
    return 0;

}

/* unlock a read lock */

static int _osns_readunlock(struct osns_lock_s *rlock)
{
    struct osns_locking_s *locking=rlock->locking;

    pthread_mutex_lock(locking->mutex);

    /* only when part of list remove it from list */

    if (rlock->flags & OSNS_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	rlock->flags &= ~OSNS_LOCK_FLAG_LIST;
	locking->readers--;
	pthread_cond_broadcast(locking->cond);

    }

    rlock->flags &= ~OSNS_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(locking->mutex);
    return 0;
}

/* full write lock */

static int _osns_writelock(struct osns_lock_s *wlock)
{
    struct osns_locking_s *locking=wlock->locking;

    wlock->thread=pthread_self();
    pthread_mutex_lock(locking->mutex);

    if ((wlock->flags & OSNS_LOCK_FLAG_LIST)==0) {

	add_list_element_last(&locking->writelocks, &wlock->list);
	locking->writers++;
	wlock->flags |= OSNS_LOCK_FLAG_LIST;

    }

    if ((wlock->flags & OSNS_LOCK_FLAG_EFFECTIVE)==0) {

	/* only get the write lock when there are no readers and this wlock is the first */

	while ((locking->readers>0) && (list_element_is_first(&wlock->list)==-1)) pthread_cond_wait(locking->cond, locking->mutex);
	wlock->flags |= OSNS_LOCK_FLAG_EFFECTIVE;

    }

    pthread_mutex_unlock(locking->mutex);
    return 0;

}

/* unlock a write lock */

static int _osns_writeunlock(struct osns_lock_s *wlock)
{
    struct osns_locking_s *locking=wlock->locking;

    pthread_mutex_lock(locking->mutex);

    if (wlock->flags & OSNS_LOCK_FLAG_LIST) {

	remove_list_element(&wlock->list);
	locking->writers--;
	wlock->flags &= ~OSNS_LOCK_FLAG_LIST;

    }

    if (wlock->flags & OSNS_LOCK_FLAG_UPGRADED) {

	locking->flags &= ~OSNS_LOCKING_FLAG_UPGRADE;
	wlock->flags &= ~OSNS_LOCK_FLAG_UPGRADED;

    }

    pthread_cond_broadcast(locking->cond);
    pthread_mutex_unlock(locking->mutex);
    wlock->flags &= ~OSNS_LOCK_FLAG_EFFECTIVE;
    return 0;
}

/* upgrade a write lock (does nothing since there is nothing "above" a write lock */

static int _osns_upgrade_writelock(struct osns_lock_s *wlock)
{
    return 0;
}

/* queue the write lock to get later a write lock
    queueing it will prevent any readers in the meantime
    when using this lock it may have become the first */

static int _osns_prewritelock(struct osns_lock_s *wlock)
{
    struct osns_locking_s *locking=wlock->locking;

    wlock->thread=pthread_self();

    if ((wlock->flags & OSNS_LOCK_FLAG_LIST)==0) {

	pthread_mutex_lock(locking->mutex);
	add_list_element_last(&locking->writelocks, &wlock->list);
	locking->writers++;
	pthread_mutex_unlock(locking->mutex);

	wlock->flags |= OSNS_LOCK_FLAG_LIST;

    }

    return 0;

}

/* upgrade a readlock to a full write lock */

static int _osns_upgrade_readlock(struct osns_lock_s *rlock)
{
    struct osns_locking_s *locking=rlock->locking;

    if (rlock->type != OSNS_LOCK_TYPE_READ) return 0;

    pthread_mutex_lock(locking->mutex);

    /* prevent two readers to upgrade at the same time */

    if (locking->flags & OSNS_LOCKING_FLAG_UPGRADE) {

	/* this readlock has to unlock hereafter, otherwise the upgrading lock will wait for this lock */
	pthread_mutex_unlock(locking->mutex);
	return -1;

    }

    locking->flags|=OSNS_LOCKING_FLAG_UPGRADE;

    if (rlock->flags & OSNS_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	locking->readers--;

    }

    /* add as first in writers list, otherwise a deadlock is possible */
    add_list_element_first(&locking->writelocks, &rlock->list);
    rlock->flags |= OSNS_LOCK_FLAG_LIST;
    locking->writers++;

    rlock->type=OSNS_LOCK_TYPE_WRITE;
    rlock->lock=_osns_writelock;
    rlock->unlock=_osns_writeunlock;
    rlock->upgrade=_osns_upgrade_writelock;
    rlock->downgrade=
    rlock->prelock=_osns_prewritelock;
    rlock->flags |= OSNS_LOCK_FLAG_UPGRADED;

    pthread_cond_broadcast(locking->cond);

    /* wait for no readers anymore (this writer is already the first) */
    while (locking->readers>0) pthread_cond_wait(locking->cond, locking->mutex);
    rlock->flags |= OSNS_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(locking->mutex);

    return 0;

}

static int _osns_prereadlock(struct osns_lock_s *rlock);

static int _osns_downgrade_readlock(struct osns_lock_s *rlock)
{
    struct osns_locking_s *locking=rlock->locking;

    if ((rlock->flags & OSNS_LOCK_FLAG_UPGRADED)==0 || (rlock->type != OSNS_LOCK_TYPE_WRITE)) return 0;

    pthread_mutex_lock(locking->mutex);

    locking->flags &= ~OSNS_LOCKING_FLAG_UPGRADE;

    if (rlock->flags & OSNS_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	locking->writers--;

    }

    add_list_element_first(&locking->readlocks, &rlock->list);
    rlock->flags |= OSNS_LOCK_FLAG_LIST;
    locking->readers++;

    rlock->type=OSNS_LOCK_TYPE_READ;
    rlock->lock=_osns_readlock;
    rlock->unlock=_osns_readunlock;
    rlock->upgrade=_osns_upgrade_readlock;
    rlock->prelock=_osns_prereadlock;
    rlock->flags &= ~OSNS_LOCK_FLAG_UPGRADED;

    pthread_cond_broadcast(locking->cond);

    rlock->flags |= OSNS_LOCK_FLAG_EFFECTIVE;
    pthread_mutex_unlock(locking->mutex);
    return 0;

}

/* turn a read lock into a pre write lock */

static int _osns_prereadlock(struct osns_lock_s *rlock)
{
    struct osns_locking_s *locking=rlock->locking;

    pthread_mutex_lock(locking->mutex);

    if (rlock->flags & OSNS_LOCK_FLAG_LIST) {

	remove_list_element(&rlock->list);
	locking->readers--;

    }

    add_list_element_first(&locking->writelocks, &rlock->list);
    locking->writers++;
    rlock->type=OSNS_LOCK_TYPE_WRITE;
    rlock->lock=_osns_writelock;
    rlock->unlock=_osns_writeunlock;
    rlock->upgrade=_osns_upgrade_writelock;
    rlock->prelock=_osns_prewritelock;

    pthread_mutex_unlock(locking->mutex);
    return 0;
}

void init_osns_nonelock(struct osns_locking_s *locking, struct osns_lock_s *lock)
{
    lock->type=OSNS_LOCK_TYPE_NONE;
    lock->thread=0;
    init_list_element(&lock->list, NULL);
    lock->flags=0;
    lock->locking=locking;
    lock->lock=_osns_nonelock;
    lock->unlock=_osns_noneunlock;
    lock->upgrade=_osns_upgrade_nonelock;
    lock->prelock=_osns_prenonelock;
}

void init_osns_readlock(struct osns_locking_s *locking, struct osns_lock_s *rlock)
{
    rlock->type=OSNS_LOCK_TYPE_READ;
    rlock->thread=0;
    init_list_element(&rlock->list, NULL);
    rlock->flags=0;
    rlock->locking=locking;
    rlock->lock=_osns_readlock;
    rlock->unlock=_osns_readunlock;
    rlock->upgrade=_osns_upgrade_readlock;
    rlock->downgrade=_osns_downgrade_readlock;
    rlock->prelock=_osns_prereadlock;
}

void init_osns_writelock(struct osns_locking_s *locking, struct osns_lock_s *wlock)
{
    wlock->type=OSNS_LOCK_TYPE_WRITE;
    wlock->thread=0;
    init_list_element(&wlock->list, NULL);
    wlock->flags=0;
    wlock->locking=locking;
    wlock->lock=_osns_writelock;
    wlock->unlock=_osns_writeunlock;
    wlock->upgrade=_osns_upgrade_writelock;
    wlock->downgrade=_osns_upgrade_writelock;
    wlock->prelock=_osns_prewritelock;
}

int osns_lock(struct osns_lock_s *lock)
{
    return (* lock->lock)(lock);
}

int osns_unlock(struct osns_lock_s *lock)
{
    return (* lock->unlock)(lock);
}

int osns_prelock(struct osns_lock_s *lock)
{
    return (* lock->prelock)(lock);
}

int osns_upgradelock(struct osns_lock_s *lock)
{
    return (* lock->upgrade)(lock);
}

int osns_downgradelock(struct osns_lock_s *lock)
{
    return (* lock->downgrade)(lock);
}

/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "log.h"

static void set_header_ops_empty(struct list_header_s *h);
static void set_header_ops_one(struct list_header_s *h);
static void set_header_ops_default(struct list_header_s *h);

static void init_list_lock(struct list_lock_s *l)
{
    l->value=0;
    l->threadidw=0;
    l->threadidpw=0;
}

static void delete_nothing(struct list_element_s *e)
{
}

struct list_element_s *get_element_system(struct list_element_s *e)
{
    return NULL;
}

struct list_element_s *get_element_default(struct list_element_s *e)
{
    return e;
}

static struct list_element_s *_get_next_element(struct list_element_s *e)
{
    struct list_element_s *next=e->n;
    return (* next->ops.get_element)(next);
}

static struct list_element_s *_get_prev_element(struct list_element_s *e)
{
    struct list_element_s *prev=e->p;
    return (* prev->ops.get_element)(prev);
}

struct list_element_s *get_next_element(struct list_element_s *e)
{
    return (e) ? _get_next_element(e) : NULL;
}

struct list_element_s *get_prev_element(struct list_element_s *e)
{
    return (e) ? _get_prev_element(e) : NULL;
}

static void _delete_element_common(struct list_header_s *h, struct list_element_s *e)
{
    struct list_element_s *n=e->n;
    struct list_element_s *p=e->p;

    n->p=p;
    p->n=n;
    init_list_element(e, NULL);

}

/* DEFAULT - DELETE */

static void delete_element_default(struct list_header_s *h, struct list_element_s *e)
{
    _delete_element_common(h, e);
    h->count--;

    if (h->count==1) {

	set_header_ops_one(h);

    } else if (h->count==0) {

	set_header_ops_empty(h);

    }

}

/* DEFAULT - INSERT AFTER */

static void insert_element_after_default(struct list_header_s *h, struct list_element_s *p, struct list_element_s *e)
{
    struct list_element_s *n=p->n;

    logoutput_debug("insert_element_after_default");

    init_list_element(e, h);

    /* insert after p (and before n) */

    n->p=e;
    p->n=e;
    e->p=p;
    e->n=n;

    h->count++;
}

/* DEFAULT - INSERT BEFORE */

static void insert_element_before_default(struct list_header_s *h, struct list_element_s *n, struct list_element_s *e)
{
    struct list_element_s *p=n->p;

    logoutput_debug("insert_element_before_default");

    init_list_element(e, h);

    /* insert before n (and before p) */

    p->n=e;
    n->p=e;
    e->n=n;
    e->p=p;

    h->count++;
}

/* HEADER ops for empty list */

static void delete_empty_header(struct list_header_s *h, struct list_element_s *e)
{
    /* does nothing since list is empty */
}

static void insert_common_empty_header(struct list_header_s *h, struct list_element_s *a, struct list_element_s *e)
{
    struct list_element_s *prev=&h->head;

    insert_element_after_default(h, prev, e);

    /* set header ops to ones suitable for a list with one element */

    set_header_ops_one(h);

}

/* NOTE: not static since used in initializer in header file (INIT_LIST_HEADER) */

struct header_ops_s empty_header_ops = {
    .insert_after				= insert_common_empty_header,
    .insert_before				= insert_common_empty_header,
    .delete					= delete_empty_header,
};

static void set_header_ops_empty(struct list_header_s *h)
{
    h->ops = &empty_header_ops;
}

/* HEADER ops for a list with one element */

static void delete_element_one(struct list_header_s *h, struct list_element_s *e)
{

    _delete_element_common(h, e);
    h->count--;
    init_list_element(e, NULL);
    set_header_ops_empty(h);
}

static void insert_after_element_one(struct list_header_s *h, struct list_element_s *a, struct list_element_s *e)
{

    insert_element_after_default(h, a, e);

    /* set header ops to ones suitable for a list with one element */

    set_header_ops_default(h);

}

static void insert_before_element_one(struct list_header_s *h, struct list_element_s *b, struct list_element_s *e)
{

    insert_element_before_default(h, b, e);

    /* set header ops to ones suitable for a list with one element */

    set_header_ops_default(h);

}

static struct header_ops_s one_list_ops = {
    .insert_after				= insert_after_element_one,
    .insert_before				= insert_before_element_one,
    .delete					= delete_element_one,
};

static void set_header_ops_one(struct list_header_s *h)
{
    h->ops = &one_list_ops;
}

/* HEADER ops for a list with more than one element */

static struct header_ops_s default_header_ops = {
    .delete				= delete_element_default,
    .insert_after			= insert_element_after_default,
    .insert_before			= insert_element_before_default,
};

static void set_header_ops_default(struct list_header_s *h)
{
    h->ops = &default_header_ops;
}

/* users functions */

void init_list_element(struct list_element_s *e, struct list_header_s *h)
{
    e->h=h;
    e->n=NULL;
    e->p=NULL;
    e->ops.get_element=get_element_default;
    init_list_lock(&e->lock);
}

void add_list_element_last(struct list_header_s *h, struct list_element_s *e)
{
    struct list_element_s *tail=&h->tail;
    init_list_element(e, h);
    (* h->ops->insert_before)(h, tail, e);
}
void add_list_element_first(struct list_header_s *h, struct list_element_s *e)
{
    struct list_element_s *head=&h->head;
    init_list_element(e, h);
    (* h->ops->insert_after)(h, head, e);
}
void add_list_element_after(struct list_header_s *h, struct list_element_s *p, struct list_element_s *e)
{
    init_list_element(e, h);
    (* h->ops->insert_after)(h, p, e);
}
void add_list_element_before(struct list_header_s *h, struct list_element_s *n, struct list_element_s *e)
{
    init_list_element(e, h);
    (* h->ops->insert_before)(h, n, e);
}
void remove_list_element(struct list_element_s *e)
{
    struct list_header_s *h=NULL;

    h=(e) ? e->h : NULL;
    if (h && h->ops) (* h->ops->delete)(h, e);
}
struct list_element_s *get_list_head(struct list_header_s *h, unsigned char flags)
{
    struct list_element_s *head=&h->head;
    struct list_element_s *e=_get_next_element(head); /* this also works when list is empty: next is tail, and get_next evaluates the get_element call which gives NULL for tail and head */
    if (e && (flags & SIMPLE_LIST_FLAG_REMOVE)) (* h->ops->delete)(h, e);
    return e;
}
struct list_element_s *get_list_tail(struct list_header_s *h, unsigned char flags)
{
    struct list_element_s *tail=&h->tail;
    struct list_element_s *e=_get_prev_element(tail);
    if (e && (flags & SIMPLE_LIST_FLAG_REMOVE)) (* h->ops->delete)(h, e);
    return e;
}
struct list_element_s *search_list_element_forw(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr)
{
    struct list_element_s *e=get_list_head(h, 0);

    while (e) {

	if (condition(e, ptr)==0) break;
	e=_get_next_element(e);

    }

    return e;
}

struct list_element_s *search_list_element_back(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr)
{
    struct list_element_s *e=get_list_tail(h, 0);

    while (e) {

	if (condition(e, ptr)==0) break;
	e=_get_prev_element(e);

    }

    return e;
}

void init_list_header(struct list_header_s *h, unsigned char type, struct list_element_s *e)
{
    struct list_element_s *head=NULL;
    struct list_element_s *tail=NULL;

    if (h==NULL) {

	logoutput_warning("init_list_header: header empty");
	return;

    } else if (h->flags & SIMPLE_LIST_FLAG_INIT) {

	logoutput_warning("init_list_header: header already initialized");
	return;

    }

    head=&h->head;
    tail=&h->tail;
    init_list_element(head, h);
    init_list_element(tail, h);

    head->n=tail;
    tail->p=head;
    head->ops.get_element=get_element_system;
    tail->ops.get_element=get_element_system;

    init_list_lock(&h->lock);
    h->readers=0;
    h->count=0;
    h->flags|=SIMPLE_LIST_FLAG_INIT;

    set_header_ops_empty(h);

    if (type==SIMPLE_LIST_TYPE_ONE) {

	if (e) {

	    add_list_element_first(h, e);

	} else {

	    logoutput_warning("init_list_header: init for a list with one element but this element is not defined");

	}

    } else if (type==SIMPLE_LIST_TYPE_DEFAULT) {

	set_header_ops_default(h);

    } else if (type!=SIMPLE_LIST_TYPE_EMPTY) {

	logoutput_warning("init_list_header: type %i not reckognized", type);

    }

}

signed char list_element_is_first(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    return (e->p==&h->head) ? 0 : -1;
}

signed char list_element_is_last(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    return (e==&h->tail) ? 0 : -1;
}

void write_lock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    unsigned char block=(SIMPLE_LIST_LOCK_WRITE | SIMPLE_LIST_LOCK_PREWRITE);
    unsigned char lock=0;

    pthread_mutex_lock(mutex);

    if ((header->lock.value & SIMPLE_LIST_LOCK_PREWRITE)==0) {

	header->lock.value |= SIMPLE_LIST_LOCK_PREWRITE;
	lock |= SIMPLE_LIST_LOCK_PREWRITE;
	block &= ~SIMPLE_LIST_LOCK_PREWRITE; /* do not block ourselves */

    }

    while ((header->lock.value & block) || header->lock.value > (SIMPLE_LIST_LOCK_WRITE | SIMPLE_LIST_LOCK_PREWRITE)) {

	pthread_cond_wait(cond, mutex);

	if ((header->lock.value & block)==0 && header->lock.value <= (SIMPLE_LIST_LOCK_WRITE | SIMPLE_LIST_LOCK_PREWRITE)) {

	    break;

	} else if ((block & SIMPLE_LIST_LOCK_PREWRITE) && (header->lock.value & SIMPLE_LIST_LOCK_PREWRITE)==0) {

	    header->lock.value |= SIMPLE_LIST_LOCK_PREWRITE;
	    lock |= SIMPLE_LIST_LOCK_PREWRITE;
	    block &= ~SIMPLE_LIST_LOCK_PREWRITE; /* do not block ourselves */

	}

    }

    header->lock.value |= SIMPLE_LIST_LOCK_WRITE;
    header->lock.value &= ~lock; /* remove the pre lock if any */
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);

}

void write_unlock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    pthread_mutex_lock(mutex);
    header->lock.value &= ~SIMPLE_LIST_LOCK_WRITE;
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);
}

void read_lock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    unsigned char block=(SIMPLE_LIST_LOCK_WRITE | SIMPLE_LIST_LOCK_PREWRITE);

    pthread_mutex_lock(mutex);
    while (header->lock.value & block) pthread_cond_wait(cond, mutex);
    header->lock.value+=SIMPLE_LIST_LOCK_READ;
    pthread_mutex_unlock(mutex);
}

void read_unlock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    pthread_mutex_lock(mutex);
    if (header->lock.value>=SIMPLE_LIST_LOCK_READ) header->lock.value-=SIMPLE_LIST_LOCK_READ;
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);
}

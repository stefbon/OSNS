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

#undef LOGGING
#include "log.h"

void set_element_ops_default_tail(struct element_ops_s *ops);
void set_element_ops_default(struct element_ops_s *ops);
void set_element_ops_default_head(struct element_ops_s *ops);
void set_element_ops_one(struct element_ops_s *ops);

static void delete_dummy(struct list_element_s *e)
{
}
static void insert_common_dummy(struct list_element_s *a, struct list_element_s *b)
{
}
void init_list_element(struct list_element_s *e, struct list_header_s *h)
{
    e->h=h;
    e->n=NULL;
    e->p=NULL;
    e->count=0;
    e->ops.delete=delete_dummy;
    e->ops.insert_after=insert_common_dummy;
    e->ops.insert_before=insert_common_dummy;
}

/* OPS for an element DEFAULT */

/* DEFAULT - INSERT BETWEEN */

static void insert_element_after_default(struct list_element_s *p, struct list_element_s *e)
{
    struct list_header_s *h=(p->h) ? p->h : e->h;

    init_list_element(e, h);

    p->n->p=e;
    p->n=e;
    e->p=p;
    e->n=p->n;

    set_element_ops_default(&e->ops);
    h->count++;
}

/* DEFAULT - INSERT after TAIL */

static void insert_element_after_tail_default(struct list_element_s *p, struct list_element_s *e)
{
    struct list_header_s *h=(p->h) ? p->h : e->h;

    init_list_element(e, h);

    p->n=e;
    e->p=p;

    set_element_ops_default(&p->ops); /* p is not the tail anymore */
    set_element_ops_default_tail(&e->ops); /* e is the tail */

    h->tail=e;
    h->count++;
}

static void insert_element_before_default(struct list_element_s *n, struct list_element_s *e)
{
    struct list_header_s *h=(n->h) ? n->h : e->h;
    struct list_element_s *p=n->p;

    init_list_element(e, h);

    p->n=e;
    n->p=e;
    e->n=n;
    e->p=p;

    set_element_ops_default(&e->ops);

    h->count++;
}

/* DEFAULT - INSERT before HEAD */

static void insert_element_before_head_default(struct list_element_s *n, struct list_element_s *e)
{
    struct list_header_s *h=(n->h) ? n->h : e->h;

    init_list_element(e, h);

    n->p=e;
    e->n=n;

    set_element_ops_default(&n->ops); /* n is not the head anymore */
    set_element_ops_default_head(&e->ops); /* e is the head */

    h->head=e;
    h->count++;
}

/* DEFAULT - DELETE between */

static void delete_element_default(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    struct list_element_s *p=e->p;
    struct list_element_s *n=e->n;

    n->p=p;
    p->n=n;

    init_list_element(e, NULL);
    h->count--;

}

static void delete_element_head(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    struct list_element_s *n=e->n; /* next */

    /* head shifts to right */

    h->head=n;
    n->p=NULL;

    /* new head has now different ops */

    h->count--;

    if (h->count==1) {

	init_list_header(h, SIMPLE_LIST_TYPE_ONE, n);
	set_element_ops_one(&n->ops);

    } else {

	set_element_ops_default_head(&n->ops);

    }

    init_list_element(e, NULL);
}

static void delete_element_tail(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    struct list_element_s *p=e->p; /* prev */

    /* head shifts to left */

    h->tail=p;
    p->n=NULL;

    /* new tail has now different ops */

    h->count--;
    if (h->count==1) {

	init_list_header(h, SIMPLE_LIST_TYPE_ONE, p);
	set_element_ops_one(&p->ops);

    } else {

	set_element_ops_default_tail(&p->ops);

    }

    init_list_element(e, NULL);
}

void set_element_ops_default(struct element_ops_s *ops)
{
    ops->delete=delete_element_default;
    ops->insert_before=insert_element_before_default;
    ops->insert_after=insert_element_after_default;
}

void set_element_ops_default_head(struct element_ops_s *ops)
{
    ops->delete=delete_element_head;
    ops->insert_before=insert_element_before_head_default;
    ops->insert_after=insert_element_after_default;
}

void set_element_ops_default_tail(struct element_ops_s *ops)
{
    ops->delete=delete_element_tail;
    ops->insert_before=insert_element_before_default;
    ops->insert_after=insert_element_after_tail_default;
}

/* OPS for an element ONE */

/* ONE */

static void delete_element_one(struct list_element_s *e)
{
    struct list_header_s *h=e->h;

    h->head=NULL;
    h->tail=NULL;

    init_list_element(e, NULL);
    h->count--;
    init_list_header(h, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

static void insert_element_before_head_one(struct list_element_s *n, struct list_element_s *e)
{
    struct list_header_s *h=(n->h) ? n->h : e->h;

    init_list_element(e, h);

    n->p=e;
    e->n=n;

    set_element_ops_default_head(&e->ops);
    set_element_ops_default_tail(&n->ops);

    h->head=e;
    h->count++;
    init_list_header(h, SIMPLE_LIST_TYPE_DEFAULT, NULL);
}

static void insert_element_after_tail_one(struct list_element_s *p, struct list_element_s *e)
{
    struct list_header_s *h=(p->h) ? p->h : e->h;

    init_list_element(e, h);

    p->n=e;
    e->p=p;

    set_element_ops_default_head(&p->ops);
    set_element_ops_default_tail(&e->ops);

    h->tail=e;
    h->count++;
    init_list_header(h, SIMPLE_LIST_TYPE_DEFAULT, NULL);
}

void set_element_ops_one(struct element_ops_s *ops)
{
    ops->delete=delete_element_one;
    ops->insert_before=insert_element_before_head_one;
    ops->insert_after=insert_element_after_tail_one;
}

/* header OPS for an EMPTY list */

static void insert_element_common_empty(struct list_element_s *a, struct list_element_s *e)
{
    /* insert after in an empty list: a must be empty
	not a is not defined but is a default parameter */

    struct list_header_s *h=e->h;

    init_list_element(e, h);
    /* start with the ONE ops */
    init_list_header(h, SIMPLE_LIST_TYPE_ONE, e);/* well it's not empty anymore */
};
static void delete_empty(struct list_element_s *e)
{
    /* delete in an empty list: not possible*/
};

void set_element_ops_empty(struct element_ops_s *ops)
{
    ops->delete=delete_empty;
    ops->insert_before=insert_element_common_empty;
    ops->insert_after=insert_element_common_empty;
}

struct header_ops_s empty_header_ops = {
    .delete				= delete_empty,
    .insert_after			= insert_element_common_empty,
    .insert_before			= insert_element_common_empty,
};

/* OPS for a NON EMPTY list */

static void insert_after_default(struct list_element_s *a, struct list_element_s *e)
{
    (* a->ops.insert_after)(a, e);
};
static void insert_before_default(struct list_element_s *b, struct list_element_s *e)
{
    (* b->ops.insert_before)(b, e);
};
static void delete_default(struct list_element_s *e)
{
    (* e->ops.delete)(e);
};

struct header_ops_s default_header_ops = {
    .delete				= delete_default,
    .insert_after			= insert_after_default,
    .insert_before			= insert_before_default,
};

void add_list_element_last(struct list_header_s *h, struct list_element_s *e)
{
    init_list_element(e, h);
    (* h->ops->insert_after)(h->tail, e);
}
void add_list_element_first(struct list_header_s *h, struct list_element_s *e)
{
    init_list_element(e, h);
    (* h->ops->insert_before)(h->head, e);
}
void add_list_element_after(struct list_header_s *h, struct list_element_s *p, struct list_element_s *e)
{
    init_list_element(e, h);
    (* h->ops->insert_after)(p, e);
}
void add_list_element_before(struct list_header_s *h, struct list_element_s *n, struct list_element_s *e)
{
    init_list_element(e, h);
    (* h->ops->insert_before)(n, e);
}
void remove_list_element(struct list_element_s *e)
{
    struct list_header_s *h=NULL;

    h=(e) ? e->h : NULL;
    if (h && h->ops) (* h->ops->delete)(e);
}
struct list_element_s *get_list_head(struct list_header_s *h, unsigned char flags)
{
    struct list_element_s *e=h->head;
    if (e && (flags & SIMPLE_LIST_FLAG_REMOVE)) (* h->ops->delete)(e);
    return e;
}
struct list_element_s *get_list_tail(struct list_header_s *h, unsigned char flags)
{
    struct list_element_s *e=h->tail;
    if (e && (flags & SIMPLE_LIST_FLAG_REMOVE)) (* h->ops->delete)(e);
    return e;
}
struct list_element_s *search_list_element_forw(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr)
{
    struct list_element_s *e=h->head;

    while (e) {

	if (condition(e, ptr)==0) break;
	e=e->n;

    }

    return e;
}

struct list_element_s *search_list_element_back(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr)
{
    struct list_element_s *e=h->tail;

    while(e) {

	if (condition(e, ptr)==0) break;
	e=e->p;

    }

    return e;
}

void init_list_header(struct list_header_s *h, unsigned char type, struct list_element_s *e)
{
    if (h==NULL) {

	logoutput_warning("init_list_header: header empty");
	return;

    }

    h->name=NULL;
    h->lock=0;
    h->readers=0;

    if (type==SIMPLE_LIST_TYPE_EMPTY) {

	h->count=0;
	h->head=NULL;
	h->tail=NULL;
	h->ops=&empty_header_ops;

    } else if (type==SIMPLE_LIST_TYPE_ONE) {

	h->count=1;
	h->head=e;
	h->tail=e;
	h->ops=&default_header_ops;

	set_element_ops_one(&e->ops);

    } else if (type==SIMPLE_LIST_TYPE_DEFAULT) {

	if (e) {

	    if (e==h->head) {

		set_element_ops_default_head(&e->ops);

	    } else if (e==h->tail) {

		set_element_ops_default_tail(&e->ops);

	    } else {

		set_element_ops_default(&e->ops);

	    }

	}

	h->ops=&default_header_ops;

    } else {

	logoutput("init_list_header: type %i not reckognized", type);

    }

}

signed char list_element_is_first(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    return (h->head==e) ? 0 : -1;
}

signed char list_element_is_last(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    return (h->tail==e) ? 0 : -1;
}

struct list_element_s *get_next_element(struct list_element_s *e)
{
    return (e) ? e->n : NULL;
}

struct list_element_s *get_prev_element(struct list_element_s *e)
{
    return (e) ? e->p : NULL;
}

void write_lock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    unsigned char block=(SIMPLE_LIST_LOCK_WRITE | SIMPLE_LIST_LOCK_PREWRITE | SIMPLE_LIST_LOCK_READ);
    unsigned char lock=0;

    pthread_mutex_lock(mutex);

    if ((header->lock & SIMPLE_LIST_LOCK_PREWRITE)==0) {

	header->lock |= SIMPLE_LIST_LOCK_PREWRITE;
	lock|=SIMPLE_LIST_LOCK_PREWRITE;
	block-=SIMPLE_LIST_LOCK_PREWRITE; /* do not block ourselves */

    }

    while (header->lock & block) {

	pthread_cond_wait(cond, mutex);

	if ((header->lock & block)==0) {

	    break;

	} else if ((block & SIMPLE_LIST_LOCK_PREWRITE) && (header->lock & SIMPLE_LIST_LOCK_PREWRITE)==0) {

	    header->lock |= SIMPLE_LIST_LOCK_PREWRITE;
	    lock|=SIMPLE_LIST_LOCK_PREWRITE;
	    block-=SIMPLE_LIST_LOCK_PREWRITE; /* do not block ourselves */

	}

    }

    header->lock |= SIMPLE_LIST_LOCK_WRITE;
    header->lock -= lock; /* remove the pre lock if any */
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);

}

void write_unlock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    pthread_mutex_lock(mutex);
    header->lock &= ~SIMPLE_LIST_LOCK_WRITE;
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);
}

void read_lock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    unsigned char block=(SIMPLE_LIST_LOCK_WRITE | SIMPLE_LIST_LOCK_PREWRITE);
    unsigned char lock=0;

    pthread_mutex_lock(mutex);
    while (header->lock & block) pthread_cond_wait(cond, mutex);
    header->lock|=SIMPLE_LIST_LOCK_READ;
    header->readers++;
    pthread_mutex_unlock(mutex);
}

void read_unlock_list_header(struct list_header_s *header, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    pthread_mutex_lock(mutex);
    header->readers--;
    if (header->readers==0) header->lock-=SIMPLE_LIST_LOCK_READ;
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mutex);
}

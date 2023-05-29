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

#include "libosns-basic-system-headers.h"

#include <pthread.h>

#include "libosns-log.h"
#include "lib/lock/signal.h"
#include "simple-list.h"

static pthread_mutex_t list_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t list_cond=PTHREAD_COND_INITIALIZER;

static struct shared_signal_s list_shared_signal = {

	.flags				= 0,
	.mutex				= &list_mutex,
	.cond				= &list_cond,
	.lock				= _signal_default_lock,
	.unlock				= _signal_default_unlock,
	.broadcast			= _signal_default_broadcast,
	.condwait			= _signal_default_condwait,
	.condtimedwait			= _signal_default_condtimedwait

};

static void set_header_ops_empty(struct list_header_s *h);
static void set_header_ops_one(struct list_header_s *h);
static void set_header_ops_default(struct list_header_s *h);

static void init_list_lock(struct list_lock_s *l)
{
    l->value=0;
    l->threadidw=0;
    l->threadidpw=0;
    l->signal=&list_shared_signal;
}

static void _rm_nothing(struct list_element_s *e)
{
}

static struct list_element_s *_get_next_element(struct list_element_s *e)
{
    struct list_element_s *n=e->n;

/*    if (n) {

        logoutput_debug("_get_next_element: next def ops %s", ((n->ops) ? n->ops->name : "nill"));

    } else {

        logoutput_debug("_get_next_element: next notdef");

    } */

    return (* n->ops->get_element)(n);
}

static struct list_element_s *_get_next_element_head(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    struct list_element_s *n=NULL;

    if (h) {

        // logoutput_debug("_get_next_element_head: h count %lu", h->count);

        if (h->count>0) {

            n=e->n;
            // logoutput_debug("_get_next_element_head: ops name next %s", n->ops->name);
            n=(* n->ops->get_element)(n);

        }

//    } else {

//        logoutput_debug("_get_next_element_head: h not defined");

    }

    return n;
}

static struct list_element_s *_get_prev_element_tail(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    struct list_element_s *p=NULL;

    if (h) {

        if (h->count>0) {

            p=e->p;
            p=(* p->ops->get_element)(p);

        }

    }

    return p;
}

struct list_element_s *_get_prev_element(struct list_element_s *e)
{
    struct list_element_s *p=e->p;
    return (* p->ops->get_element)(p);
}

static struct list_element_s *_get_element_default(struct list_element_s *e)
{
    return e;
}

static void _rm_default(struct list_element_s *e)
{
    struct list_header_s *h=e->h;
    if (h && h->ops) (* h->ops->delete)(h, e);
}

/* system head element ops */

static struct list_element_s *_get_element_empty(struct list_element_s *e)
{
    return NULL;
}

struct list_element_ops_s system_head_element_ops = {
    .name                                       = "H",
    .get_element                                = _get_element_empty,
    .get_next                                   = _get_next_element_head,
    .get_prev                                   = _get_element_empty,
    .rm_element                                 = _rm_nothing,
};

/* system tail element ops */

struct list_element_ops_s system_tail_element_ops = {
    .name                                       = "T", 
    .get_element                                = _get_element_empty,
    .get_next                                   = _get_element_empty,
    .get_prev                                   = _get_prev_element_tail,
    .rm_element                                 = _rm_nothing,
};

/* stand alone element */

struct list_element_ops_s standalone_element_ops = {
    .name                                       = "S",
    .get_element                                = _get_element_default,
    .get_next                                   = _get_element_empty,
    .get_prev                                   = _get_element_empty,
    .rm_element                                 = _rm_nothing,
};

/* default */

struct list_element_ops_s default_element_ops = {
    .name                                       = "D",
    .get_element                                = _get_element_default,
    .get_next                                   = _get_next_element,
    .get_prev                                   = _get_prev_element,
    .rm_element                                 = _rm_default,
};

struct list_element_s *get_next_element(struct list_element_s *e)
{
//     logoutput_debug("get_next_element: ops name %s", e->ops->name);
    return (* e->ops->get_next)(e);
}

struct list_element_s *get_prev_element(struct list_element_s *e)
{
    return (* e->ops->get_prev)(e);
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

    insert_element_after_default(h, &h->head, e);

    /* set header ops to ones suitable for a list with one element */

    set_header_ops_one(h);

}

static struct list_element_s *get_head_empty_list(struct list_header_s *h)
{
    return NULL;
}

/* NOTE: not static since used in initializer in header file (INIT_LIST_HEADER) */

struct list_header_ops_s empty_header_ops = {
    .insert_after				= insert_common_empty_header,
    .insert_before				= insert_common_empty_header,
    .delete					= delete_empty_header,
    .get_head                                   = get_head_empty_list,
    .get_tail                                   = get_head_empty_list,
};

static void set_header_ops_empty(struct list_header_s *h)
{
    h->ops = &empty_header_ops;
}

struct list_header_ops_s *get_empty_header_ops()
{
    return &empty_header_ops;
}

/* HEADER ops for a list with one element */

static void delete_element_one(struct list_header_s *h, struct list_element_s *e)
{

    _delete_element_common(h, e);
    h->count--;
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

static struct list_element_s *get_head_default(struct list_header_s *h)
{
    struct list_element_s *head=&h->head;
    return head->n;
}

static struct list_element_s *get_tail_default(struct list_header_s *h)
{
    struct list_element_s *tail=&h->tail;
    return tail->p;
}

static struct list_header_ops_s one_list_ops = {
    .insert_after				= insert_after_element_one,
    .insert_before				= insert_before_element_one,
    .delete					= delete_element_one,
    .get_head                                   = get_head_default,
    .get_tail                                   = get_tail_default,
};

static void set_header_ops_one(struct list_header_s *h)
{
    h->ops = &one_list_ops;
}

/* HEADER ops for a list with more than one element */

static struct list_header_ops_s default_header_ops = {
    .delete				        = delete_element_default,
    .insert_after			        = insert_element_after_default,
    .insert_before			        = insert_element_before_default,
    .get_head                                   = get_head_default,
    .get_tail                                   = get_tail_default,
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
    init_list_lock(&e->lock);

    if (h) {

        e->ops=&default_element_ops;
        e->lock.signal=h->lock.signal;

    } else {

        e->ops=&standalone_element_ops;

    }

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
    if (e && e->ops) (* e->ops->rm_element)(e);
}
struct list_element_s *get_list_head(struct list_header_s *h)
{
    return (* h->ops->get_head)(h);
}
struct list_element_s *remove_list_head(struct list_header_s *h)
{
    struct list_element_s *e=get_list_head(h);
    remove_list_element(e);
    return e;
}

struct list_element_s *get_list_tail(struct list_header_s *h)
{
    return (* h->ops->get_tail)(h);
}

struct list_element_s *remove_list_tail(struct list_header_s *h)
{
    struct list_element_s *e=get_list_tail(h);
    remove_list_element(e);
    return e;
}

struct list_element_s *search_list_element_forw(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr)
{
    struct list_element_s *e=get_list_head(h);

    while (e) {

	if (condition(e, ptr)==0) break;
	e=_get_next_element(e);

    }

    return e;
}

struct list_element_s *search_list_element_back(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr)
{
    struct list_element_s *e=get_list_tail(h);

    while (e) {

	if (condition(e, ptr)==0) break;
	e=_get_prev_element(e);

    }

    return e;
}

void cb_lock_dummy(struct list_header_s *h, unsigned char action)
{}

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

    head->n=tail;
    head->p=tail;
    head->h=h;
    head->ops=&system_head_element_ops;
    init_list_lock(&head->lock);

    tail->p=head;
    tail->n=head;
    tail->h=h;
    tail->ops=&system_tail_element_ops;
    init_list_lock(&tail->lock);

    init_list_lock(&h->lock);
    h->cb_lock=cb_lock_dummy;
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

void set_lock_cb_list_header(struct list_header_s *h, void (* cb)(struct list_header_s *h, unsigned char action))
{
    h->cb_lock=cb;
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

void write_lock_list_header(struct list_header_s *h)
{
    write_lock_list(&h->lock);
}

void write_unlock_list_header(struct list_header_s *h)
{
    write_unlock_list(&h->lock);
}

void read_lock_list_header(struct list_header_s *h)
{
    read_lock_list(&h->lock);
}

void read_unlock_list_header(struct list_header_s *h)
{
    read_unlock_list(&h->lock);
}

void write_lock_list_element(struct list_element_s *l)
{
    write_lock_list(&l->lock);
}

void write_unlock_list_element(struct list_element_s *l)
{
    write_unlock_list(&l->lock);
}

void read_lock_list_element(struct list_element_s *l)
{
    read_lock_list(&l->lock);
}

void read_unlock_list_element(struct list_element_s *l)
{
    read_unlock_list(&l->lock);
}

void wait_lock_list(struct list_lock_s *l, unsigned char (* condition)(void *ptr), void *ptr)
{
    signal_lock(l->signal);
    while (condition(ptr)==0) signal_condwait(l->signal);
    signal_unlock(l->signal);
}

void set_signal_list_header(struct list_header_s *h, struct shared_signal_s *signal)
{
    h->lock.signal=signal;
}

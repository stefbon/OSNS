/*

  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#ifndef _LIB_LIST_SIMPLE_LIST_H
#define _LIB_LIST_SIMPLE_LIST_H

#include					<stdint.h>
#include 					"lock.h"

#define SIMPLE_LIST_TYPE_EMPTY			1
#define SIMPLE_LIST_TYPE_DEFAULT		2
#define SIMPLE_LIST_TYPE_ONE			3

#define SIMPLE_LIST_FLAG_REMOVE			1
#define SIMPLE_LIST_FLAG_INIT			2

struct shared_signal_s;
struct list_element_s;
struct list_header_s;

struct list_element_ops_s {
    const char                                                  *name;
    struct list_element_s	                                *(* get_element)(struct list_element_s *e);
    struct list_element_s	                                *(* get_next)(struct list_element_s *e);
    struct list_element_s	                                *(* get_prev)(struct list_element_s *e);
    void                                                        (* rm_element)(struct list_element_s *e);
};

extern struct list_element_s *get_element_default(struct list_element_s *e);

struct list_header_ops_s {
    void			                                (* insert_after)(struct list_header_s *h, struct list_element_s *a, struct list_element_s *e);
    void			                                (* insert_before)(struct list_header_s *h, struct list_element_s *b, struct list_element_s *e);
    void 			                                (* delete)(struct list_header_s *h, struct list_element_s *e);
    struct list_element_s	                                *(* get_head)(struct list_header_s *h);
    struct list_element_s	                                *(* get_tail)(struct list_header_s *h);
};

struct list_element_s {
    struct list_header_s	                                *h;
    struct list_element_s 	                                *n;
    struct list_element_s 	                                *p;
    struct list_element_ops_s	                                *ops;
    struct list_lock_s		                                lock;
};

#define SIMPLE_LIST_LOCK_ACTION_RLOCK				1
#define SIMPLE_LIST_LOCK_ACTION_WLOCK				2
#define SIMPLE_LIST_LOCK_ACTION_RUNLOCK				3
#define SIMPLE_LIST_LOCK_ACTION_WUNLOCK				4

extern void cb_lock_dummy(struct list_header_s *h, unsigned char action);

struct list_header_s {
    unsigned int		                                flags;
    uint64_t			                                count;
    void			                                (* cb_lock)(struct list_header_s *h, unsigned char action);
    void			                                *ptr;
    struct list_lock_s		                                lock;
    struct list_element_s 	                                head;
    struct list_element_s 	                                tail;
    struct list_header_ops_s		                        *ops;
};

extern struct list_header_ops_s empty_header_ops;
extern struct list_element_ops_s standalone_element_ops;

#define				INIT_LIST_ELEMENT		{.h=NULL, .n=NULL, .p=NULL, .ops=&standalone_element_ops, .lock.value=0, .lock.threadidw=0, .lock.threadidpw=0}
#define				INIT_LIST_HEADER		{.flags=0, .count=0, .cb_lock=cb_lock_dummy, .ptr=NULL, .lock.value=0, .lock.threadidw=0, .lock.threadidpw=0, .head=INIT_LIST_ELEMENT, .tail=INIT_LIST_ELEMENT, NULL}

void init_list_element(struct list_element_s *e, struct list_header_s *h);
void init_list_header(struct list_header_s *h, unsigned char type, struct list_element_s *e);

void add_list_element_last(struct list_header_s *h, struct list_element_s *e);
void add_list_element_first(struct list_header_s *h, struct list_element_s *e);
void add_list_element_after(struct list_header_s *h, struct list_element_s *p, struct list_element_s *e);
void add_list_element_before(struct list_header_s *h, struct list_element_s *n, struct list_element_s *e);
void remove_list_element(struct list_element_s *e);

struct list_element_s *get_list_head(struct list_header_s *h);
struct list_element_s *get_list_tail(struct list_header_s *h);

struct list_element_s *remove_list_head(struct list_header_s *h);
struct list_element_s *remove_list_tail(struct list_header_s *h);

struct list_element_s *search_list_element_forw(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr);
struct list_element_s *search_list_element_back(struct list_header_s *h, int (* condition)(struct list_element_s *list, void *ptr), void *ptr);

signed char list_element_is_first(struct list_element_s *e);
signed char list_element_is_last(struct list_element_s *e);

struct list_element_s *get_next_element(struct list_element_s *e);
struct list_element_s *get_prev_element(struct list_element_s *e);

void set_lock_cb_list_header(struct list_header_s *h, void (* cb)(struct list_header_s *h, unsigned char action));

void write_lock_list_header(struct list_header_s *header);
void write_unlock_list_header(struct list_header_s *header);
void read_lock_list_header(struct list_header_s *header);
void read_unlock_list_header(struct list_header_s *header);

void write_lock_list_element(struct list_element_s *l);
void write_unlock_list_element(struct list_element_s *l);
void read_lock_list_element(struct list_element_s *l);
void read_unlock_list_element(struct list_element_s *l);

void wait_lock_list(struct list_lock_s *l, unsigned char (* condition)(void *ptr), void *ptr);
void set_signal_list_header(struct list_header_s *h, struct shared_signal_s *signal);

#endif

/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_SL_SKIPLIST_H
#define _LIB_SL_SKIPLIST_H

#include "libosns-list.h"

#define _SKIPLIST_VERSION_MAJOR				1
#define _SKIPLIST_VERSION_MINOR				0
#define _SKIPLIST_VERSION_PATCH				0

#define _SKIPLIST_PROB					4
#define _SKIPLIST_MAXLANES				10

/* lock set on an entire dirnode when exact match is found by delete */
#define _DIRNODE_LOCK_RM				1

/* lock set in a lane/region to inserte/remove an entry */
#define _DIRNODE_LOCK_PREWRITE				2
#define _DIRNODE_LOCK_WRITE				4

/* lock set in a lane/region to read */
#define _DIRNODE_LOCK_READ				8

#define _SKIPLIST_FLAG_ALLOC				1

#define _DIRNODE_FLAG_START				1
#define _DIRNODE_FLAG_ALLOC				2

#define _SL_OPCODE_FIND					1
#define _SL_OPCODE_INSERT				2
#define _SL_OPCODE_DELETE				3

#define _SL_MOVE_FLAG_DELETE				1
#define _SL_MOVE_FLAG_INSERT				2
#define _SL_MOVE_FLAG_REMOVE				4
#define _SL_MOVE_FLAG_MOVELEFT				8
#define _SL_MOVE_FLAG_MOVERIGHT				16
#define _SL_MOVE_FLAG_DIRNODERIGHT			32

struct sl_skiplist_s;

struct sl_junction_s {
    unsigned  						step;
    struct sl_dirnode_s 				*n;
    struct sl_dirnode_s 				*p;
    unsigned int					count;
};

struct sl_dirnode_s {
    struct list_element_s 				*list;
    unsigned char					flags;
    unsigned char					lock;
    unsigned int					lockers;
    unsigned short					level;
    int							(* compare)(struct list_element_s *l, void *b);
    struct sl_junction_s				junction[];
};

struct sl_move_dirnode_s {
    unsigned char					flags;
    struct sl_dirnode_s					*dirnode;
    unsigned int					left;
    unsigned int					right;
    int							step;
    struct list_element_s				*list;
};

struct sl_ops_s {
    int 						(* compare)(struct list_element_s *l, void *b);
    struct list_element_s				*(* get_list_element)(void *lookupdata, struct sl_skiplist_s *sl);
    char						*(* get_logname)(struct list_element_s *data);
};

struct sl_skiplist_s {
    struct sl_ops_s					ops;
    unsigned 						prob;
    unsigned int					flags;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
    unsigned short					maxlevel;
    struct list_header_s				header;
    void						*ptr;
    unsigned int					size;
    struct sl_dirnode_s					dirnode;
};

struct sl_path_s {
    struct sl_dirnode_s 				*dirnode;
    unsigned char					lockset;
    unsigned char					lock;
    unsigned int 					step;
};

struct sl_vector_s {
    unsigned short					maxlevel;
    unsigned short					minlevel;
    unsigned char					lockset;
    unsigned short					level;
    struct sl_path_s 					path[];
};

struct sl_lockops_s {
	void						(* remove_lock_vector)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, void (* cb)(struct sl_skiplist_s *sl, struct sl_vector_s *v, unsigned int l, struct sl_move_dirnode_s *m), struct sl_move_dirnode_s *m);
	void						(* readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode);
	void						(* remove_readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector);
	void						(* move_readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *next);
	int						(* upgrade_readlock_vector)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned char move);
};

#define SL_SEARCHRESULT_FLAG_FIRST				(1 << 0)
#define SL_SEARCHRESULT_FLAG_LAST				(1 << 1)
#define SL_SEARCHRESULT_FLAG_EXACT				(1 << 2)
#define SL_SEARCHRESULT_FLAG_BEFORE				(1 << 3)
#define SL_SEARCHRESULT_FLAG_AFTER				(1 << 4)
#define SL_SEARCHRESULT_FLAG_NOENT				(1 << 5)
#define SL_SEARCHRESULT_FLAG_DIRNODE				(1 << 6)
#define SL_SEARCHRESULT_FLAG_EMPTY				(1 << 7)
#define SL_SEARCHRESULT_FLAG_EXCLUSIVE				(1 << 8)
#define SL_SEARCHRESULT_FLAG_OK					(1 << 9)
#define SL_SEARCHRESULT_FLAG_ERROR				(1 << 20)

struct sl_searchresult_s {
    void						*lookupdata;
    struct list_element_s				*found;
    unsigned int					flags;
    unsigned int					ctr;
    unsigned int					row;
    unsigned int					step;
};

/* prototypes */

int init_sl_skiplist(struct sl_skiplist_s *sl,
		    int (* compare) (struct list_element_s *l, void *b),
		    struct list_element_s *(* get_list_element) (void *lookupdata, struct sl_skiplist_s *sl),
		    char *(* get_logname)(struct list_element_s *l), void *ptr);

struct sl_skiplist_s *create_sl_skiplist(struct sl_skiplist_s *sl, unsigned char prob, unsigned int size, unsigned char maxlanes);
unsigned int get_size_sl_skiplist(unsigned char *p_maxlanes);

void clear_sl_skiplist(struct sl_skiplist_s *sl);
void free_sl_skiplist(struct sl_skiplist_s *sl);

void init_sl_searchresult(struct sl_searchresult_s *result, void *lookupdata, unsigned int flags);
unsigned short get_default_sl_prob();
unsigned char estimate_sl_lanes(uint64_t count, unsigned prob);

#endif

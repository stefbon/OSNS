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

#ifndef LIB_LOCK_LOCKING_H
#define LIB_LOCK_LOCKING_H

#include "libosns-list.h"

#define OSNS_LOCK_TYPE_NONE		0
#define OSNS_LOCK_TYPE_READ		1
#define OSNS_LOCK_TYPE_WRITE		2

#define OSNS_LOCK_FLAG_LIST		1
#define OSNS_LOCK_FLAG_WAITING		2
#define OSNS_LOCK_FLAG_EFFECTIVE	4
#define OSNS_LOCK_FLAG_ALLOCATED	8
#define OSNS_LOCK_FLAG_UPGRADED		16

#define OSNS_LOCKING_FLAG_ALLOC_LOCK	1
#define OSNS_LOCKING_FLAG_ALLOC_MUTEX 	2
#define OSNS_LOCKING_FLAG_ALLOC_COND	4

#define OSNS_LOCKING_FLAG_UPGRADE	8

struct osns_locking_s {
    unsigned int			flags;
    pthread_mutex_t			*mutex;
    pthread_cond_t			*cond;
    struct list_header_s		readlocks;
    unsigned int			readers;
    struct list_header_s		writelocks;
    unsigned int			writers;
};

struct osns_lock_s {
    unsigned char			type;
    struct list_element_s		list;
    pthread_t				thread;
    unsigned char			flags;
    struct osns_locking_s		*locking;
    int					(* lock)(struct osns_lock_s *l);
    int					(* unlock)(struct osns_lock_s *l);
    int					(* upgrade)(struct osns_lock_s *l);
    int					(* downgrade)(struct osns_lock_s *l);
    int					(* prelock)(struct osns_lock_s *l);
};

/* prototypes */

void init_osns_nonelock(struct osns_locking_s *locking, struct osns_lock_s *lock);
void init_osns_readlock(struct osns_locking_s *locking, struct osns_lock_s *rlock);
void init_osns_writelock(struct osns_locking_s *locking, struct osns_lock_s *wlock);

int init_osns_locking(struct osns_locking_s *locking, unsigned int flags);
void clear_osns_locking(struct osns_locking_s *locking);

int osns_lock(struct osns_lock_s *lock);
int osns_unlock(struct osns_lock_s *lock);
int osns_prelock(struct osns_lock_s *lock);
int osns_upgradelock(struct osns_lock_s *lock);
int osns_downgradelock(struct osns_lock_s *lock);

#endif

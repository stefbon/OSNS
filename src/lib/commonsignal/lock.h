/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_COMMONSIGNAL_LOCK_H
#define _LIB_COMMONSIGNAL_LOCK_H

#include <pthread.h>

#define COMMON_SIGNAL_FLAG_ALLOC			1
#define COMMON_SIGNAL_FLAG_DEFAULT			2

struct common_signal_s {
    unsigned int					flags;
    pthread_mutex_t					*mutex;
    pthread_cond_t					*cond;
    int							(* lock)(struct common_signal_s *s);
    int							(* unlock)(struct common_signal_s *s);
    int							(* broadcast)(struct common_signal_s *s);
    int							(* condwait)(struct common_signal_s *s);
    int							(* condtimedwait)(struct common_signal_s *s, struct timespec *expire);
    char						buffer[];
};

/* prototypes */

struct common_signal_s *create_custom_common_signal();
struct common_signal_s *get_default_common_signal();

int signal_lock(struct common_signal_s *s);
int signal_unlock(struct common_signal_s *s);

int signal_broadcast(struct common_signal_s *s);
int signal_condwait(struct common_signal_s *s);
int signal_condtimedwait(struct common_signal_s *s, struct timespec *t);

void clear_common_signal(struct common_signal_s **p_s);

#endif

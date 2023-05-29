/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Stef Bon <stefbon@gmail.com>

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
#include "signal.h"

static pthread_mutex_t			default_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t			default_cond=PTHREAD_COND_INITIALIZER;

/* DEFAULT SIGNAL */

int _signal_default_lock(struct shared_signal_s *s)
{
    return pthread_mutex_lock(s->mutex);
}

int _signal_default_unlock(struct shared_signal_s *s)
{
    return pthread_mutex_unlock(s->mutex);
}

int _signal_default_broadcast(struct shared_signal_s *s)
{
    return pthread_cond_broadcast(s->cond);
}

int _signal_default_condwait(struct shared_signal_s *s)
{
    return pthread_cond_wait(s->cond, s->mutex);
}

int _signal_default_condtimedwait(struct shared_signal_s *s, struct system_timespec_s *t)
{
    struct timespec expire={.tv_sec=t->st_sec, .tv_nsec=t->st_nsec};
    return pthread_cond_timedwait(s->cond, s->mutex, &expire);
}

static struct shared_signal_s default_shared_signal = {

	.flags				= SHARED_SIGNAL_FLAG_DEFAULT,
	.mutex				= &default_mutex,
	.cond				= &default_cond,
	.lock				= _signal_default_lock,
	.unlock				= _signal_default_unlock,
	.broadcast			= _signal_default_broadcast,
	.condwait			= _signal_default_condwait,
	.condtimedwait			= _signal_default_condtimedwait

};

struct shared_signal_s *get_default_shared_signal()
{
    return &default_shared_signal;
}

int _signal_none_lock(struct shared_signal_s *s)
{
    return 0;
}

int _signal_none_unlock(struct shared_signal_s *s)
{
    return 0;
}

int _signal_none_broadcast(struct shared_signal_s *s)
{
    return 0;
}

int _signal_none_condwait(struct shared_signal_s *s)
{
    return 0;
}

int _signal_none_condtimedwait(struct shared_signal_s *s, struct system_timespec_s *t)
{
    return 0;
}

static struct shared_signal_s none_shared_signal = {

	.flags				= SHARED_SIGNAL_FLAG_NONE,
	.mutex				= NULL,
	.cond				= NULL,
	.lock				= _signal_none_lock,
	.unlock				= _signal_none_unlock,
	.broadcast			= _signal_none_broadcast,
	.condwait			= _signal_none_condwait,
	.condtimedwait			= _signal_none_condtimedwait

};

struct shared_signal_s *get_none_shared_signal()
{
    return &none_shared_signal;
}

void set_custom_shared_signal_none(struct shared_signal_s *signal)
{
	signal->flags			        = SHARED_SIGNAL_FLAG_NONE;
	signal->lock				= _signal_none_lock;
	signal->unlock				= _signal_none_unlock;
	signal->broadcast			= _signal_none_broadcast;
	signal->condwait			= _signal_none_condwait;
	signal->condtimedwait			= _signal_none_condtimedwait;
}

void set_custom_shared_signal_default(struct shared_signal_s *signal, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    signal->mutex 			= mutex;
    signal->cond  			= cond;
    signal->lock  			= _signal_default_lock;
    signal->unlock 			= _signal_default_unlock;
    signal->broadcast			= _signal_default_broadcast;
    signal->condwait			= _signal_default_condwait;
    signal->condtimedwait		= _signal_default_condtimedwait;
}

void set_custom_shared_signal(struct shared_signal_s *signal, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    signal->flags=0;
    set_custom_shared_signal_default(signal, mutex, cond);
}

struct shared_signal_s *create_custom_shared_signal()
{
    unsigned size=sizeof(struct shared_signal_s) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t);
    struct shared_signal_s *signal=malloc(size);

    if (signal) {
	pthread_mutex_t *mutex=NULL;
	pthread_cond_t *cond=NULL;

	memset(signal, 0, size);

	mutex = (pthread_mutex_t *) signal->buffer;
 	cond  = (pthread_cond_t *) (signal->buffer + sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex, NULL);
	pthread_cond_init(cond, NULL);

	signal->flags = SHARED_SIGNAL_FLAG_ALLOC | SHARED_SIGNAL_FLAG_ALLOC_MUTEX | SHARED_SIGNAL_FLAG_ALLOC_COND;
 	set_custom_shared_signal_default(signal, mutex, cond);

    }

    return signal;
}

struct shared_signal_s *get_custom_shared_signal(pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    struct shared_signal_s *signal=malloc(sizeof(struct shared_signal_s));

    if (signal) {

	signal->flags = SHARED_SIGNAL_FLAG_ALLOC;
	set_custom_shared_signal_default(signal, mutex, cond);

    }

    return signal;
}


int signal_lock(struct shared_signal_s *s)
{
    return (* s->lock)(s);
}
int signal_unlock(struct shared_signal_s *s)
{
    return (* s->unlock)(s);
}
int signal_broadcast(struct shared_signal_s *s)
{
    return (* s->broadcast)(s);
}
int signal_condwait(struct shared_signal_s *s)
{
    return (* s->condwait)(s);
}
int signal_condtimedwait(struct shared_signal_s *s, struct system_timespec_s *t)
{
    return (* s->condtimedwait)(s, t);
}

void clear_shared_signal(struct shared_signal_s **p_s)
{
    struct shared_signal_s *s=*p_s;

    if ((s->flags & SHARED_SIGNAL_FLAG_DEFAULT)==0) {

	if (s->flags & SHARED_SIGNAL_FLAG_ALLOC_MUTEX) pthread_mutex_destroy(s->mutex);
	if (s->flags & SHARED_SIGNAL_FLAG_ALLOC_COND) pthread_cond_destroy(s->cond);

	if (s->flags & SHARED_SIGNAL_FLAG_ALLOC) {

	    free(s);
	    *p_s=NULL;

	}

    }

}

void signal_broadcast_locked(struct shared_signal_s *s)
{
    signal_lock(s);
    signal_broadcast(s);
    signal_unlock(s);
}

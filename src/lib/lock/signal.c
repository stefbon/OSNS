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

int default_lock(struct shared_signal_s *s)
{
    return pthread_mutex_lock(s->mutex);
}

int default_unlock(struct shared_signal_s *s)
{
    return pthread_mutex_unlock(s->mutex);
}

int default_broadcast(struct shared_signal_s *s)
{
    return pthread_cond_broadcast(s->cond);
}

int default_condwait(struct shared_signal_s *s)
{
    return pthread_cond_wait(s->cond, s->mutex);
}

int default_condtimedwait(struct shared_signal_s *s, struct system_timespec_s *t)
{
    struct timespec expire={.tv_sec=t->st_sec, .tv_nsec=t->st_nsec};
    return pthread_cond_timedwait(s->cond, s->mutex, &expire);
}

static struct shared_signal_s default_shared_signal = {

	.flags				= SHARED_SIGNAL_FLAG_DEFAULT,
	.mutex				= &default_mutex,
	.cond				= &default_cond,
	.lock				= default_lock,
	.unlock				= default_unlock,
	.broadcast			= default_broadcast,
	.condwait			= default_condwait,
	.condtimedwait			= default_condtimedwait

};

struct shared_signal_s *get_default_shared_signal()
{
    return &default_shared_signal;
}

struct shared_signal_s *create_custom_shared_signal()
{
    unsigned size=sizeof(struct shared_signal_s) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t);
    struct shared_signal_s *signal=malloc(size);

    if (signal) {

	memset(signal, 0, size);

	signal->flags 			= SHARED_SIGNAL_FLAG_ALLOC | SHARED_SIGNAL_FLAG_ALLOC_MUTEX | SHARED_SIGNAL_FLAG_ALLOC_COND;
	signal->mutex 			= (pthread_mutex_t *) signal->buffer;
 	signal->cond  			= (pthread_cond_t *) (signal->buffer + sizeof(pthread_mutex_t));
	signal->lock  			= default_lock;
	signal->unlock 			= default_unlock;
	signal->broadcast		= default_broadcast;
	signal->condwait		= default_condwait;
	signal->condtimedwait		= default_condtimedwait;

	pthread_mutex_init(signal->mutex, NULL);
	pthread_cond_init(signal->cond, NULL);

    }

    return signal;
}

struct shared_signal_s *get_custom_shared_signal(pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    struct shared_signal_s *signal=malloc(sizeof(struct shared_signal_s));

    if (signal) {

	signal->flags 			= SHARED_SIGNAL_FLAG_ALLOC;
	signal->mutex 			= mutex;
 	signal->cond  			= cond;
	signal->lock  			= default_lock;
	signal->unlock 			= default_unlock;
	signal->broadcast		= default_broadcast;
	signal->condwait		= default_condwait;
	signal->condtimedwait		= default_condtimedwait;

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

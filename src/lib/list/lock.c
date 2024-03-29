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

#include "libosns-log.h"
#include "signal.h"
#include "lock.h"

void write_lock_list(struct list_lock_s *lock)
{
    struct shared_signal_s *signal=lock->signal;
    unsigned char block=(SIMPLE_LOCK_WRITE | SIMPLE_LOCK_PREWRITE);
    unsigned char tmp=0;

    // logoutput_debug("write_lock_list: A1 tid %u", gettid());

    signal_lock(signal);

    if ((lock->value & SIMPLE_LOCK_PREWRITE)==0 || lock->threadidpw==pthread_self()) {

	lock->value |= SIMPLE_LOCK_PREWRITE;
	tmp |= SIMPLE_LOCK_PREWRITE;
	block &= ~SIMPLE_LOCK_PREWRITE; /* do not block ourselves */
	lock->threadidpw=pthread_self();

    }

    while ((lock->value & block) || (lock->value > (SIMPLE_LOCK_WRITE | SIMPLE_LOCK_PREWRITE))) {

	signal_condwait(signal);

	if ((lock->value & block)==0 && (lock->value <= (SIMPLE_LOCK_WRITE | SIMPLE_LOCK_PREWRITE))) {

	    break;

	} else if ((block & SIMPLE_LOCK_PREWRITE) && (lock->value & SIMPLE_LOCK_PREWRITE)==0) {

	    lock->value |= SIMPLE_LOCK_PREWRITE;
	    tmp |= SIMPLE_LOCK_PREWRITE;
	    block &= ~SIMPLE_LOCK_PREWRITE; /* do not block ourselves */
	    lock->threadidpw=pthread_self();

	}

    }

    lock->value |= SIMPLE_LOCK_WRITE;
    lock->value &= ~tmp; /* remove the pre lock if any */
    lock->threadidw=pthread_self();
    lock->threadidpw=0;

    signal_broadcast(signal);
    signal_unlock(signal);

    // logoutput_debug("write_lock_list: A2 tid %u", gettid());

}

void write_unlock_list(struct list_lock_s *lock)
{
    struct shared_signal_s *signal=lock->signal;

    signal_lock(signal);
    lock->value &= ~SIMPLE_LOCK_WRITE;
    lock->threadidw=0;
    signal_broadcast(signal);
    signal_unlock(signal);
}

void read_lock_list(struct list_lock_s *lock)
{
    struct shared_signal_s *signal=lock->signal;
    unsigned char block=(SIMPLE_LOCK_WRITE | SIMPLE_LOCK_PREWRITE);

    signal_lock(signal);
    while (lock->value & block) signal_condwait(signal);
    lock->value+=SIMPLE_LOCK_READ;
    signal_unlock(signal);
}

void read_unlock_list(struct list_lock_s *lock)
{
    struct shared_signal_s *signal=lock->signal;

    signal_lock(signal);
    if (lock->value>=SIMPLE_LOCK_READ) lock->value-=SIMPLE_LOCK_READ;
    signal_broadcast(signal);
    signal_unlock(signal);
}

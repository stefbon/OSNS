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

#include "libosns-basic-system-headers.h"

#include "libosns-misc.h"
#include "libosns-log.h"
#include "signal.h"

static unsigned char wait_flag_set_cb(unsigned int *p_flags, unsigned int flag)
{
    return ((*p_flags & flag) ? 1 : 0);
}

static unsigned char wait_flag_unset_cb(unsigned int *p_flags, unsigned int flag)
{
    return ((*p_flags & flag) ? 0 : 1);
}

static int signal_wait_flag_common(struct shared_signal_s *signal, unsigned int *p_flags, unsigned int flag, struct system_timespec_s *expire, unsigned char (* condition)(unsigned int *p_flags, unsigned int flag))
{
    int result=-1;

    signal_lock(signal);

    while ((* condition)(p_flags, flag)==0) {

	if (expire) {

	    result=signal_condtimedwait(signal, expire);
	    if (result==ETIMEDOUT) break;

	} else {

	    result=signal_condwait(signal);

	}

    }

    signal_unlock(signal);
    return ((* condition)(p_flags, flag) ? 0 : result);

}

int signal_wait_flag_set(struct shared_signal_s *signal, unsigned int *p_flags, unsigned int flag, struct system_timespec_s *expire)
{
    return signal_wait_flag_common(signal, p_flags, flag, expire, wait_flag_set_cb);
}

int signal_wait_flag_unset(struct shared_signal_s *signal, unsigned int *p_flags, unsigned int flag, struct system_timespec_s *expire)
{
    return signal_wait_flag_common(signal, p_flags, flag, expire, wait_flag_unset_cb);
}

unsigned int signal_set_flag(struct shared_signal_s *signal, unsigned int *p_flags, unsigned int flag)
{
    unsigned int result=0;
    unsigned int flags=0;

    signal_lock(signal);
    flags=*p_flags;
    result=(flag & ~flags); /* is the flag not set? so the set will actually do something */
    *p_flags |= flag;
    signal_broadcast(signal);
    signal_unlock(signal);

    return result;
}

unsigned int signal_unset_flag(struct shared_signal_s *signal, unsigned int *p_flags, unsigned int flag)
{
    unsigned int result=0;
    unsigned int flags=0;

    signal_lock(signal);
    flags=*p_flags;
    result=(flag & flags); /* is flag set? so the unset will actually do something? */
    *p_flags &= ~flag;
    signal_broadcast(signal);
    signal_unlock(signal);

    return result;
}

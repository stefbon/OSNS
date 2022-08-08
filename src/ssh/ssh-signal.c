/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-utils.h"

void init_ssh_signal(struct ssh_signal_s *signal, unsigned int flags, struct shared_signal_s *s)
{
    signal->flags=flags;
    signal->signal=s;
    signal->sequence_number_error=0;
    signal->error=0;
}

int ssh_signal_lock(struct ssh_signal_s *signal)
{
    return signal_lock(signal->signal);
}

int ssh_signal_unlock(struct ssh_signal_s *signal)
{
    return signal_unlock(signal->signal);
}

int ssh_signal_broadcast(struct ssh_signal_s *signal)
{
    return signal_broadcast(signal->signal);
}

int ssh_signal_condwait(struct ssh_signal_s *signal)
{
    return signal_condwait(signal->signal);
}

int ssh_signal_condtimedwait(struct ssh_signal_s *signal, struct system_timespec_s *t)
{
    return signal_condtimedwait(signal->signal, t);
}


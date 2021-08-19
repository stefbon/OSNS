/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define LOGGING
#include "log.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "commonsignal.h"

#include "smb.h"
#include "smb-signal.h"

int smb_signal_lock(struct smb_signal_s *s)
{
    return signal_lock(s->signal);
}

int smb_signal_unlock(struct smb_signal_s *s)
{
    return signal_unlock(s->signal);
}

int smb_signal_condtimedwait(struct smb_signal_s *s, struct timespec *e)
{
    return signal_condtimedwait(s->signal, e);
}

int smb_signal_broadcast(struct smb_signal_s *s)
{
    return signal_broadcast(s->signal);
}


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

#include <linux/fs.h>

#include "main.h"
#include "log.h"
#include "misc.h"

#include "workspace-interface.h"

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-hostinfo.h"

#include "sftp/common-protocol.h"
#include "common.h"
#include "time.h"

/*
    function to correct the timestamps in the various attributes fields */

static void correct_time_ignore(struct sftp_client_s *sftp, struct timespec *time)
{
    /* does nothing */
}

void init_sftp_timecorrection(struct sftp_client_s *sftp)
{
    sftp->time_ops.correct_time_s2c=correct_time_ignore;
    sftp->time_ops.correct_time_c2s=correct_time_ignore;
}

void correct_time_s2c(struct sftp_client_s *sftp, struct timespec *time)
{
    (* sftp->time_ops.correct_time_s2c)(sftp, time);
}

void correct_time_c2s(struct sftp_client_s *sftp, struct timespec *time)
{
    (* sftp->time_ops.correct_time_c2s)(sftp, time);
}

void enable_timecorrection(struct sftp_client_s *sftp)
{

    if ((* sftp->context.signal_sftp2conn)(sftp, "command:timecorrection:", NULL) >= 0) {

	logoutput("enable_timecorrection: send command timecorrection to connection");

    } else {

	logoutput("enable_timecorrection: error sending command timecorrection");

    }
}

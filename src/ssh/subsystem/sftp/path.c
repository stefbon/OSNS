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
#include <sys/vfs.h>
#include <pwd.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "attributes-write.h"
#include "send.h"
#include "path.h"
#include "handle.h"
#include "init.h"

unsigned int get_fullpath_len(struct sftp_identity_s *user, unsigned int len, char *buffer)
{
    if (len==0 || buffer[0]!='/') return len + 1 + user->len_home;
    return len;
}

/* note: this does not handle UTF encoding/decoding */

void get_fullpath(struct sftp_identity_s *user, unsigned int len, char *buffer, char *path)
{

    if (len==0 || buffer[0]!='/') {
	unsigned int index=0;

	memcpy(&path[index], user->pwd.pw_dir, user->len_home);
	index+=user->len_home;
	path[index]='/';
	index++;
	memcpy(&path[index], buffer, len); /* this works also when len==0 */
	index+=len;
	path[index]='\0';

    } else {

	memcpy(path, &buffer[0], len);
	path[len]='\0';

    }

}

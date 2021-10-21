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
#include "system.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "path.h"

void path_append_home_directory(struct sftp_identity_s *user, struct ssh_string_s *path, struct fs_location_path_s *localpath)
{
    char *ptr=localpath->ptr;

    memcpy(ptr, user->pwd.pw_dir, user->len_home);
    ptr+=user->len_home;
    *ptr='/';
    ptr++;

    /* TODO: add convert from UTF-8 to local encoding */

    memcpy(ptr, path->ptr, path->len);
    ptr+=path->len;
    *ptr='\0';

}

void path_append_none(struct sftp_identity_s *user, struct ssh_string_s *path, struct fs_location_path_s *localpath)
{
    char *ptr=localpath->ptr;

    memcpy(ptr, path->ptr, path->len);
    ptr+=path->len;
    *ptr='\0';

}

unsigned int get_fullpath_size(struct sftp_identity_s *user, struct ssh_string_s *path, struct convert_sftp_path_s *convert)
{
    char *ptr=path->ptr;

    if (path->len>0 && ptr[0]=='/') {

	convert->complete=&path_append_none;
	return path->len;

    }

    convert->complete=&path_append_home_directory;
    return path->len + 1 + user->len_home;

}

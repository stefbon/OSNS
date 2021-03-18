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
#include <sys/vfs.h>
#include <pwd.h>
#include <grp.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "mountinfo.h"

#include "misc.h"
#include "osns_sftp_subsystem.h"
#include "receive.h"

static pthread_mutex_t user_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t group_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t uid_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gid_mutex=PTHREAD_MUTEX_INITIALIZER;

unsigned int get_username(uid_t uid, struct ssh_string_s *user)
{
    struct passwd *pwd=NULL;

    pthread_mutex_lock(&user_mutex);
    pwd=getpwuid(uid);

    if (pwd) {

	user->len=strlen(pwd->pw_name);
	user->ptr=malloc(user->len);

	if (user->ptr) {

	    memcpy(user->ptr, pwd->pw_name, user->len);

	} else {

	    user->len=0;

	}

    }

    pthread_mutex_unlock(&user_mutex);

    return user->len;
}

unsigned int get_groupname(gid_t gid, struct ssh_string_s *group)
{
    struct group *grp=NULL;

    pthread_mutex_lock(&group_mutex);
    grp=getgrgid(gid);

    if (grp) {

	group->len=strlen(grp->gr_name);
	group->ptr=malloc(group->len);

	if (group->ptr) {

	    memcpy(group->ptr, grp->gr_name, group->len);

	} else {

	    group->len=0;

	}

    }

    pthread_mutex_unlock(&group_mutex);

    return group->len;
}

/* simple functions to do a lookup of the names
    (no mapping) */

uid_t read_sftp_owner(struct ssh_string_s *user, unsigned int *flags)
{
    uid_t result=(uid_t) -1; /* TODO: a specific unknown user */
    char buffer[user->len + 1];
    struct passwd *pwd=NULL;

    memcpy(buffer, user->ptr, user->len);
    buffer[user->len]='\0';

    pthread_mutex_lock(&uid_mutex);
    pwd=getpwnam(buffer);

    if (pwd) {

	result=pwd->pw_uid;
	*flags|=SFTP_ATTR_FLAG_VALIDUSER;

    } else {

	*flags|=SFTP_ATTR_FLAG_USERNOTFOUND;

    }

    pthread_mutex_unlock(&uid_mutex);
    return result;
}

uid_t unknown_sftp_owner()
{
    return (uid_t) -1;
}

gid_t read_sftp_group(struct ssh_string_s *group, unsigned int *flags)
{
    gid_t result=(gid_t) -1; /* TODO: a specific unknown group */
    char buffer[group->len + 1];
    struct group *grp=NULL;

    memcpy(buffer, group->ptr, group->len);
    buffer[group->len]='\0';

    pthread_mutex_lock(&gid_mutex);
    grp=getgrnam(buffer);

    if (grp) {

	result=grp->gr_gid;
	*flags|=SFTP_ATTR_FLAG_VALIDGROUP;

    } else {

	*flags|=SFTP_ATTR_FLAG_GROUPNOTFOUND;

    }

    pthread_mutex_unlock(&gid_mutex);
    return result;
}

gid_t unknown_sftp_group()
{
    return (gid_t) -1;
}

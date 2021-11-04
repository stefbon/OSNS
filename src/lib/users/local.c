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

#include "pwd.h"
#include "grp.h"

#include "log.h"
#include "main.h"
#include "misc.h"
#include "datatypes.h"

static pthread_mutex_t pwd_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t grp_mutex=PTHREAD_MUTEX_INITIALIZER;

void lock_local_userbase()
{
    int result=pthread_mutex_lock(&pwd_mutex);
}

void get_local_uid_byname(char *name, uid_t *uid, unsigned int *error)
{
    struct passwd *pwd=getpwnam(name);

    if (pwd) {

	*error=0;
	*uid=pwd->pw_uid;

    } else {

	*error=ENOENT;

    }

}

void get_local_uid_byid(uid_t id, uid_t *uid, unsigned int *error)
{
    struct passwd *pwd=getpwuid(id);

    if (pwd) {

	*uid=id;

    } else {

	*error=ENOENT;

    }

}

void get_local_user_byuid(uid_t uid, void (* cb_found)(char *name, void *ptr), void (* cb_notfound)(void *ptr), void *ptr)
{
    struct passwd *pwd=getpwuid(uid);

    if (pwd) {

	(* cb_found)(pwd->pw_name, ptr);

    } else {

	(* cb_notfound)(ptr);

    }

}

void unlock_local_userbase()
{
    int result=pthread_mutex_unlock(&pwd_mutex);
}

void lock_local_groupbase()
{
    int result=pthread_mutex_lock(&grp_mutex);
}

void get_local_gid_byname(char *name, gid_t *gid, unsigned int *error)
{
    struct group *grp=getgrnam(name);

    if (grp) {

	*error=0;
	*gid=grp->gr_gid;

    } else {

	*error=ENOENT;

    }

}

void get_local_gid_byid(gid_t id, gid_t *gid, unsigned int *error)
{
    struct group *grp=getgrgid(id);

    if (grp) {

	*gid=id;

    } else {

	*error=ENOENT;

    }

}

void get_local_group_bygid(gid_t gid, void (* cb_found)(char *name, void *ptr), void (* cb_notfound)(void *ptr), void *ptr)
{
    struct group *grp=getgrgid(gid);

    if (grp) {

	(* cb_found)(grp->gr_name, ptr);

    } else {

	(* cb_notfound)(ptr);

    }

}

void unlock_local_groupbase()
{
    pthread_mutex_unlock(&grp_mutex);
}

unsigned char user_is_groupmember(char *username, struct group *grp)
{
    unsigned char found=0;
    char **member=grp->gr_mem;

    while(*member && found==0) {

	if (strcmp(username, *member)==0) {

	    found=1;

	} else {

	    member++;

	}

    }

    return found;

}

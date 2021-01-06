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

#include "logging.h"
#include "main.h"

#include "utils.h"
#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "users/local.h"

/* get local uid */

static void get_local_uid_shared_byid(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    logoutput("get_local_uid_shared_byid");
    user->uid=user->remote.id;
}

static void get_local_uid_shared_byname(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;
    uid_t uid=usermapping->unknown_uid;
    char name[user->remote.name.len + 1];
    char *sep=NULL;

    /* name can contain a @ sign for the domain */
    memcpy(name, user->remote.name.ptr, user->remote.name.len);
    name[user->remote.name.len]='@';
    sep=rawmemchr(name, '@'); /* this will never fail since terminated with an at sign */
    *sep='\0';

    lock_local_userbase();
    get_local_uid_byname(name, &user->uid);
    unlock_local_userbase();
}

static void get_local_uid_nonshared_byid(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;

    user->uid=usermapping->unknown_uid;

    if (user->remote.id==usermapping->remote_user.type.user.uid) {

	user->uid=usermapping->uid;
	logoutput("get_local_uid_nonshared_byid: A %i", user->uid);

    } else if (user->remote.id==0) {

	user->uid=0;
	logoutput("get_local_uid_nonshared_byid: B %i", user->uid);

    /* TODO: find out about remote users by name and compare to local ones: map these uid's */

    }

}

static void get_local_uid_nonshared_byname(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;
    unsigned int len=user->remote.name.len;
    char name[len + 1];
    char *sep=NULL;

    user->uid=usermapping->unknown_uid;

    memcpy(name, user->remote.name.ptr, len);
    name[len]='@';
    /* only look at the first part without the domain ... */
    sep=rawmemchr(name, '@');
    *sep='\0';
    len=(unsigned int) (sep - name);

    if (len==usermapping->remote_user.len && memcmp(name, usermapping->remote_user.name, len)==0) {

	user->uid=usermapping->uid;

    } else if (len==4 && memcmp(name, "root", 4)==0) {

	user->uid=0;

    } else {

	/* fallback on lookup the name */

	lock_local_userbase();
	get_local_uid_byname(name, &user->uid);
	unlock_local_userbase();

    }

}

/* get local gid */

static void get_local_gid_shared_byid(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    group->gid=group->remote.id;
}

static void get_local_gid_shared_byname(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;
    char name[group->remote.name.len + 1];
    char *sep=NULL;

    group->gid=usermapping->unknown_gid;

    memcpy(name, group->remote.name.ptr, group->remote.name.len);
    name[group->remote.name.len]='@';
    sep=rawmemchr(name, '@');
    *sep='\0';

    lock_local_groupbase();
    get_local_gid_byname(name, &group->gid);
    unlock_local_groupbase();

}

static void get_local_gid_nonshared_byid(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;

    group->gid=usermapping->unknown_gid;

    if (group->remote.id==usermapping->remote_group.type.group.gid) {

	group->gid=usermapping->pwd.pw_gid;

    } else if (group->remote.id==0) {

	group->gid=0;
	/* TODO: find out about remote groups by name and compare to local ones: map these gid's */

    }

}

static void get_local_gid_nonshared_byname(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;
    unsigned int len=group->remote.name.len;
    char name[len + 1];
    char *sep=NULL;

    group->gid=usermapping->unknown_gid;

    memcpy(name, group->remote.name.ptr, len);
    name[len]='@';
    /* only look at the first part without the domain ... */
    sep=rawmemchr(name, '@');
    *sep='\0';
    len=(unsigned int) (sep - name);

    if (len==usermapping->remote_group.len && memcmp(name, usermapping->remote_group.name, len)==0) {

	group->gid=usermapping->pwd.pw_gid;

    } else if (group->remote.name.len==4 && memcmp(name, "root", 4)==0) {

	group->gid=0;

    } else {

	lock_local_groupbase();
	get_local_gid_byname(name, &group->gid);
	unlock_local_groupbase();

    }

}

/* get remote uid */

static void get_remote_uid_shared(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    user->remote.id=user->uid;
}

static void get_remote_uid_nonshared(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;

    user->remote.id=usermapping->remote_user.type.user.uid;

    if (user->uid==0) {

	user->remote.id=0;

    } else if (user->uid==usermapping->uid) {

	user->remote.id=usermapping->remote_user.type.user.uid;

    /* more id's ? */


    }

}

/* get remote gid */

static void get_remote_gid_shared(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    group->remote.id=group->gid;
}

static void get_remote_gid_nonshared(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;

    if (group->gid==0) {

	group->remote.id=0;

    } else if (group->gid==usermapping->pwd.pw_gid) {

	group->remote.id=usermapping->remote_group.type.group.gid;

    /* more id's ? */

    }

}

/* get remote username (using local uid) */

static void get_remote_username_shared(struct sftp_client_s *sftp, struct sftp_user_s *user)
{
    lock_local_userbase();
    get_local_user_byuid(user->uid, &user->remote.name);
    unlock_local_userbase();
}

static void get_remote_username_nonshared(struct sftp_client_s *sftp, struct sftp_user_s *user)
{

    if (user->uid==0) {

	user->remote.name.len=4;
	if (user->remote.name.ptr) memcpy(user->remote.name.ptr, "root", user->remote.name.len);

    } else {
	struct sftp_usermapping_s *usermapping=&sftp->usermapping;

	/* find out about remote groups
	    for now map everyting to the connecting user */

	user->remote.name.len=usermapping->remote_user.len;
	if (user->remote.name.ptr) memcpy(user->remote.name.ptr, usermapping->remote_user.name, user->remote.name.len);

    }

}

/* get remote groupname (using local gid) */

static void get_remote_groupname_shared(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    lock_local_groupbase();
    get_local_group_bygid(group->gid, &group->remote.name);
    unlock_local_groupbase();
}

static void get_remote_groupname_nonshared(struct sftp_client_s *sftp, struct sftp_group_s *group)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;

    if (group->gid==0) {

	group->remote.name.len=4;
	if (group->remote.name.ptr) memcpy(group->remote.name.ptr, "root", group->remote.name.len);

    } else {

	/* find out about remote groups
	    for now map everyting to the connecting user/group */

	group->remote.name.len=usermapping->remote_group.len;
	if (group->remote.name.ptr) memcpy(group->remote.name.ptr, usermapping->remote_group.name, group->remote.name.len);

    }

}

void set_usermapping(struct sftp_client_s *sftp)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;

    if (usermapping->mapping==_SFTP_USER_MAPPING_SHARED) {

	if (sftp->server_version<=3) {

	    /* users and groups are communicated via uid and gid */

	    usermapping->get_local_uid=get_local_uid_shared_byid;
	    usermapping->get_local_gid=get_local_gid_shared_byid;

	    usermapping->get_remote_user=get_remote_uid_shared;
	    usermapping->get_remote_group=get_remote_gid_shared;

	} else {

	    /* users and groups are communicated via username and groupname */

	    usermapping->get_local_uid=get_local_uid_shared_byname;
	    usermapping->get_local_gid=get_local_gid_shared_byname;

	    usermapping->get_remote_user=get_remote_username_shared;
	    usermapping->get_remote_group=get_remote_groupname_shared;

	}

    } else {

	/* user- and groupnames do not have a common id database like openldap */

	if (sftp->server_version<=3) {

	    usermapping->get_local_uid=get_local_uid_nonshared_byid;
	    usermapping->get_local_gid=get_local_gid_nonshared_byid;

	    usermapping->get_remote_user=get_remote_uid_nonshared;
	    usermapping->get_remote_group=get_remote_gid_nonshared;

	} else {

	    /* users and groups are communicated via username and groupname */

	    usermapping->get_local_uid=get_local_uid_nonshared_byname;
	    usermapping->get_local_gid=get_local_gid_nonshared_byname;

	    usermapping->get_remote_user=get_remote_username_nonshared;
	    usermapping->get_remote_group=get_remote_groupname_nonshared;

	}

    }

}

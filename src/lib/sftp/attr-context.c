/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#include "log.h"
#include "misc.h"

#include "network.h"
#include "users.h"
#include "common-protocol.h"
#include "attr-context.h"

static void get_local_uid_bysftpid(struct attr_context_s *ctx, struct sftp_user_s *user)
{
    user->uid=(uid_t) user->remote.id;
}

static void get_local_gid_bysftpid(struct attr_context_s *ctx, struct sftp_group_s *group)
{
    group->gid=(gid_t) group->remote.id;
}

static void get_local_uid_bysftpname(struct attr_context_s *ctx, struct sftp_user_s *user)
{
    unsigned int len=user->remote.name.len;
    char name[len + 2];
    char *sep=NULL;

    memcpy(name, user->remote.name.ptr, len);
    *(name + len)='@';
    *(name + len + 1)='\0';
    sep=rawmemchr(name, '@');
    *sep='\0';
    *(name + len)='\0';

    /* what to do with domain ?*/

    // domain->ptr=sep+1;
    // domain->len=(unsigned int)(name + len - sep);

    lock_local_userbase();
    get_local_uid_byname(name, &user->uid);
    unlock_local_userbase();
}

static void get_local_gid_bysftpname(struct attr_context_s *ctx, struct sftp_group_s *group)
{
    unsigned int len=group->remote.name.len;
    char name[len + 2];
    char *sep=NULL;

    memcpy(name, group->remote.name.ptr, len);
    *(name + len)='@';
    *(name + len + 1)='\0';
    sep=rawmemchr(name, '@');
    *sep='\0';
    *(name + len)='\0';

    /* what to do with domain ?*/

    //domain->ptr=sep+1;
    //domain->len=(unsigned int)(name + len - sep);

    lock_local_groupbase();
    get_local_gid_byname(name, &group->gid);
    unlock_local_groupbase();
}

static void get_remote_uid_bysftpid(struct attr_context_s *ctx, struct sftp_user_s *user)
{
    user->remote.id=user->uid;
}

static void get_remote_gid_bysftpid(struct attr_context_s *ctx, struct sftp_group_s *group)
{
    group->remote.id=group->gid;
}

static void get_remote_username_bysftpid(struct attr_context_s *ctx, struct sftp_user_s *user)
{
    lock_local_userbase();
    get_local_user_byuid(user->uid, &user->remote.name);
    unlock_local_userbase();
}

static void get_remote_groupname_bysftpid(struct attr_context_s *ctx, struct sftp_group_s *group)
{
    lock_local_groupbase();
    get_local_group_bygid(group->gid, &group->remote.name);
    unlock_local_groupbase();
}


/* TODO: mapping based on username and (optional) domain */

/*
    base the domainname, if none then "localdomain"

    example.org/{aname, bname, cname, anothername, etcname, etcetera}

    rules:
	- the user used to connect with the server
	(which maybe different from the username on the client)
	is mapped to the local user on the client
	- root is mapped to root
	- default users like nobody are mapped to their equivalents
	- (optional) a list of usernames on server using a special command like:
	    getent passwd | grep ".*:.*:.*:100:"
	    where 100 is the gid of the group of desktopusers and users to connect to this srver

*/

static unsigned int get_maxlength_username(struct attr_context_s *ctx)
{
    return 32;
}

static unsigned int get_maxlength_groupname(struct attr_context_s *ctx)
{
    return 32;
}

static unsigned int get_maxlength_domainname(struct attr_context_s *ctx)
{
    return HOST_HOSTNAME_FQDN_MAX_LENGTH;
}

static unsigned char get_sftp_protocol_version(struct attr_context_s *ctx)
{
    return 0;
}

static unsigned int get_sftp_flags(struct attr_context_s *ctx, const char *what)
{
    return 0;
}

void init_attr_context(struct attr_context_s *ctx, void *ptr)
{

    ctx->get_local_uid_byid=get_local_uid_bysftpid;
    ctx->get_local_gid_byid=get_local_gid_bysftpid;

    ctx->get_local_uid_byname=get_local_uid_bysftpname;
    ctx->get_local_gid_byname=get_local_gid_bysftpname;

    ctx->get_remote_uid_byid=get_remote_uid_bysftpid;
    ctx->get_remote_gid_byid=get_remote_gid_bysftpid;

    ctx->get_remote_username_byid=get_remote_username_bysftpid;
    ctx->get_remote_groupname_byid=get_remote_groupname_bysftpid;

    ctx->maxlength_username=get_maxlength_username;
    ctx->maxlength_groupname=get_maxlength_groupname;
    ctx->maxlength_domainname=get_maxlength_domainname;

    ctx->get_sftp_protocol_version=get_sftp_protocol_version;
    ctx->get_sftp_flags=get_sftp_flags;

    ctx->ptr=ptr;

}
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

#include <pwd.h>
#include <grp.h>

#include "logging.h"
#include "main.h"
#include "misc.h"

#include "threads.h"
#include "workspace-interface.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "user/mapping.h"

/*
    get the uid and gid of the unknown user and group
    uid and gid of the remote host which are not reckognized
    (== not the connecting user/group, not root and not nobody...)
    get this uid/gid
*/

static void get_local_unknown_user(struct sftp_client_s *sftp)
{
    struct sftp_usermapping_s *users=&sftp->usermapping;
    struct passwd *pwd=NULL;
    char *user=NULL;
    struct ctx_option_s option;

    users->unknown_uid=(uid_t) -1;
    users->unknown_gid=(gid_t) -1;

    memset(&option, 0, sizeof(struct ctx_option_s));
    if ((* sftp->context.signal_sftp2ctx)(sftp, "option:sftp.usermapping.user-unknown", &option)>=0) {

	if (option.type==_CTX_OPTION_TYPE_PCHAR) user=option.value.name;

    }

    pwd=(user ? getpwnam(user) : getpwnam("unknown"));

    if (pwd) {

	logoutput("get_local_unknown_user: user %i:%s", pwd->pw_uid, pwd->pw_name);
	users->unknown_uid=pwd->pw_uid;
	users->unknown_gid=pwd->pw_gid;
	return;

    }

    /* take nobody */

    user=NULL;
    memset(&option, 0, sizeof(struct ctx_option_s));
    if ((* sftp->context.signal_sftp2ctx)(sftp, "option:sftp.usermapping.user-nobody", &option)>=0) {

	if (option.type==_CTX_OPTION_TYPE_PCHAR) user=option.value.name;

    }


    pwd=((user) ? getpwnam(user) : NULL);

    if (pwd) {

	logoutput("get_local_unknown_user: user %i:%s", pwd->pw_uid, pwd->pw_name);
	users->unknown_uid=pwd->pw_uid;
	users->unknown_gid=pwd->pw_gid;
	return;

    }

    pwd=getpwnam("nobody");

    if (pwd) {

	logoutput("get_local_unknown_user: user %i:%s", pwd->pw_uid, pwd->pw_name);
	users->unknown_uid=pwd->pw_uid;
	users->unknown_gid=pwd->pw_gid;
	return;

    }

    logoutput("get_local_unknown_user: no user found ");

}

static void init_getent_fields(struct getent_fields_s *fields)
{
    memset(fields, 0, sizeof(struct getent_fields_s));
    fields->flags=0;
    init_ssh_string(&fields->getent);
    fields->name=NULL;
    fields->len=0;
}

static void free_getent_fields(struct getent_fields_s *fields)
{
    clear_ssh_string(&fields->getent);
}

static char *get_next_field(char *pos, int *p_size)
{
    int size=*p_size;
    char *sep=memchr(pos, ':', size);

    if (sep) {

	*sep='\0';
	sep++;
	size-=(unsigned int)(sep-pos);
	*p_size=size;

    }

    return sep;
}

static int get_getent_fields(char *getent, unsigned int size, struct getent_fields_s *fields)
{
    char *pos=getent;
    int left=(int) size;
    char *tmp=NULL;

    /* name */
    fields->name=pos;
    pos=get_next_field(pos, &left);
    fields->len=strlen(fields->name);

    /* ignore x */
    pos=get_next_field(pos, &left);

    /* gid (group) or uid (user) */

    tmp=pos;
    pos=get_next_field(pos, &left);

    if (fields->flags & GETENT_TYPE_GROUP) {

	fields->type.group.gid=(gid_t) atoi(tmp);
	return 0;

    } else {

	fields->type.user.uid=(uid_t) atoi(tmp);

    }

    /* gid for user */

    tmp=pos;
    pos=get_next_field(pos, &left);
    fields->type.user.uid=(uid_t) atoi(tmp);

    /* gecos */

    fields->type.user.fullname=pos;
    pos=get_next_field(pos, &left);

    /* home */

    fields->type.user.home=pos;
    pos=get_next_field(pos, &left);

    return 0;

}

static int get_sftp_userinfo(struct sftp_client_s *sftp)
{
    struct ctx_option_s option;
    unsigned int size;
    struct sftp_usermapping_s *users=&sftp->usermapping;
    struct passwd *result=NULL;

    /* get the local user properties and store in pwd */

    users->buffer=NULL;
    users->size=128;

    getpw:

    memset(&users->pwd, 0, sizeof(struct passwd));
    result=NULL;

    users->buffer=realloc(users->buffer, users->size);
    if (users->buffer==NULL) goto error;

    if (getpwuid_r(users->uid, &users->pwd, users->buffer, users->size, &result)==-1) {

	if (errno==ERANGE) {

	    users->size+=128;
	    goto getpw; /* size buffer too small, increase and try again */

	}

	logoutput("get_sftp_userinfo: failed to signal connection");
	goto error;

    }

    /* get the remote user properties and store in fields */

    memset(&option, 0, sizeof(struct ctx_option_s));
    if ((* sftp->context.signal_sftp2conn)(sftp, "info:getentuser:", &option)>=0) {

	/* reply is of command getent passwd $USER */

	if (option.type==_CTX_OPTION_TYPE_BUFFER) {
	    struct getent_fields_s *ugetent=&users->remote_user;

	    ugetent->flags |= GETENT_TYPE_USER;

	    // replace_cntrl_char(option.value.buffer.ptr, option.value.buffer.len, REPLACE_CNTRL_FLAG_TEXT);

	    if (get_getent_fields(option.value.buffer.ptr, option.value.buffer.len, ugetent)==0) {

		set_ssh_string(&ugetent->getent, option.value.buffer.len, option.value.buffer.ptr);
		option.value.buffer.ptr=NULL;
		option.value.buffer.len=0;

		logoutput("get_sftp_userinfo: user %i - %s:%s %s", ugetent->type.user.uid, ugetent->name, ugetent->type.user.fullname, ugetent->type.user.home);

	    } else {

		logoutput("get_sftp_userinfo: failed to get fields from remote user");
		goto error;

	    }

	} else {

	    logoutput("get_sftp_userinfo: reply from connection not a buffer");
	    goto error;

	}

    } else {

	logoutput("get_sftp_userinfo: failed to signal connection");
	goto error;

    }

    /* get the remote group properties and store in fields */

    memset(&option, 0, sizeof(struct ctx_option_s));
    if ((* sftp->context.signal_sftp2conn)(sftp, "info:getentgroup:", &option)>=0) {

	/* reply is of command getent group $(id -g) */

	if (option.type==_CTX_OPTION_TYPE_BUFFER) {
	    struct getent_fields_s *ggetent=&users->remote_group;

	    ggetent->flags |= GETENT_TYPE_GROUP;
	    // replace_cntrl_char(option.value.buffer.ptr, option.value.buffer.len, REPLACE_CNTRL_FLAG_TEXT);

	    if (get_getent_fields(option.value.buffer.ptr, option.value.buffer.len, ggetent)==0) {

		set_ssh_string(&ggetent->getent, option.value.buffer.len, option.value.buffer.ptr);
		option.value.buffer.ptr=NULL;
		option.value.buffer.len=0;

		logoutput("get_sftp_userinfo: group %i - %s", ggetent->type.group.gid, ggetent->name);

	    } else {

		logoutput("get_sftp_userinfo: failed to get fields from remote group");
		goto error;

	    }

	} else {

	    logoutput("get_sftp_userinfo: reply from connection not a buffer");
	    goto error;

	}

    } else {

	logoutput("get_sftp_userinfo: failed to signal connection");
	goto error;

    }

    out:
    return 0;

    error:
    return -1;

}

static unsigned char get_sftp_user_mapping(struct sftp_client_s *sftp)
{
    struct ctx_option_s option;
    unsigned char mapping=_SFTP_USER_MAPPING_SHARED;

    /* TODO: add "sending" of the name of the remote host */

    memset(&option, 0, sizeof(struct ctx_option_s));
    if ((* sftp->context.signal_sftp2ctx)(sftp, "option:sftp.usermapping.type", &option)>=0) {

	if (option.type==_CTX_OPTION_TYPE_PCHAR) {

	    if (strcmp(option.value.name, "none")==0) {

		/* no mapping or translation required, uid and gid are shared via ldap/ad for example  */

		mapping=_SFTP_USER_MAPPING_SHARED;

	    } else if (strcmp(option.value.name, "map")==0) {

		/* simple mapping is used like:
		    local user 	<-> remote user
		    root		<-> root
		    nobody		<-> nobody
		    everything else mapped to the unknown user */

		mapping=_SFTP_USER_MAPPING_NONSHARED;

	    } else if (strcmp(option.value.name, "file")==0) {

		/* there is a file with remote users mapped to local users
		    for now handle this as simple mapping */

		mapping=_SFTP_USER_MAPPING_NONSHARED;

	    } else {

		logoutput_warning("get_sftp_user_mapping: option sftp.usermapping.type value %s not reckognized", option.value.name);

	    }

	}

    } else {

	logoutput_warning("get_sftp_user_mapping: option sftp.usermapping.type not reckognized");

    }

    return mapping;
}

void init_sftp_usermapping(struct sftp_client_s *sftp, uid_t uid)
{
    struct sftp_usermapping_s *users=&sftp->usermapping;
    unsigned char mapping=_SFTP_USER_MAPPING_NONSHARED;

    logoutput("init_sftp_usermapping: uid %i", uid);

    memset(users, 0, sizeof(struct sftp_usermapping_s));
    users->type=0;
    users->uid=uid;
    users->mapping=0;
    users->buffer=NULL;
    users->size=0;
    init_getent_fields(&users->remote_user);
    init_getent_fields(&users->remote_group);
    users->unknown_uid=(uid_t) -1;
    users->unknown_gid=(gid_t) -1;

}

int set_sftp_usermapping(struct sftp_client_s *sftp)
{

    get_local_unknown_user(sftp);
    get_sftp_userinfo(sftp);

    /* is the user id db shared with server? (via ldap etc.) */

    sftp->usermapping.mapping=get_sftp_user_mapping(sftp);

    /* set the right calls to translate remote users to local versions, using id's and/or names
	and vica versa */

    set_usermapping(sftp);

    return 0;

}

void free_sftp_usermapping(struct sftp_client_s *sftp)
{
    struct sftp_usermapping_s *users=&sftp->usermapping;

    if (users->buffer) {

	free(users->buffer);
	users->buffer=NULL;

    }

    free_getent_fields(&users->remote_user);
    free_getent_fields(&users->remote_group);
}

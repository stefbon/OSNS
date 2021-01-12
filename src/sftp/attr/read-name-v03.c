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

#include "log.h"
#include "main.h"

#include "misc.h"
#include "log.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"
#include "read-attr-v03.h"

static unsigned int type_mapping[5]={0, S_IFREG, S_IFDIR, S_IFLNK, 0};

/*
    read a name and attributes from a name response
    for version 4 a name response looks like:

    uint32				id
    uint32				count
    repeats count times:
	string				filename
	string				longname
	ATTRS				attr


    longname is output of ls -l command like:

    -rwxr-xr-x   1 mjos     staff      348911 Mar 25 14:29 t-filexfer
    1234567890 123 12345678 12345678 12345678 123456789012
    01234567890123456789012345678901234567890123456789012345
    0         1         2         3         4         5

    example:

    -rw-------    1 sbon     sbon         1799 Nov 26 15:22 .bash_history


*/

static void _dummy_cb(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
}

void read_name_nameresponse_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct ssh_string_s *name)
{
    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, name, _dummy_cb, NULL);
    logoutput_debug("read_attr_response_v03: name %.*s", name->len, name->ptr);
}

void read_attr_nameresponse_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct ssh_string_s longname=SSH_STRING_INIT;
    unsigned char correction=0;

    /* longname */

    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &longname, _dummy_cb, NULL);
    logoutput_debug("read_attr_response_v03: longname %.*s", longname.len, longname.ptr);

    /* attr */

    (*sftp->attr_ops.read_attributes)(sftp, buffer, attr);

    /* get type from longname: parse the string which is output of ls -al %filename%
	and others like user, group and size if not from attr already */

    switch (longname.ptr[0]) {

	case '-':

	    attr->type |= S_IFREG;
	    break;

	case 'd':

	    attr->type |= S_IFDIR;
	    break;

	case 'l':

	    attr->type |= S_IFLNK;
	    break;

	case 'c':

	    attr->type |= S_IFCHR;
	    break;

	case 'b':

	    attr->type |= S_IFBLK;
	    break;

	case 's':

	    attr->type |= S_IFSOCK;
	    break;

	default:

	    attr->type |= S_IFREG;

    }

    logoutput_debug("read_attr_response_v03: type %i", attr->type);
    attr->received|=SFTP_ATTR_TYPE;

    if ((attr->received & SFTP_ATTR_PERMISSIONS)==0) {

	/* only get permissions from longname if not already from attr */

	attr->permissions|=(longname.ptr[1]=='r') ? S_IRUSR : 0;
	attr->permissions|=(longname.ptr[2]=='w') ? S_IWUSR : 0;
	attr->permissions|=(longname.ptr[3]=='x') ? S_IXUSR : 0;

	attr->permissions|=(longname.ptr[4]=='r') ? S_IRGRP : 0;
	attr->permissions|=(longname.ptr[5]=='w') ? S_IWGRP : 0;
	attr->permissions|=(longname.ptr[6]=='x') ? S_IXGRP : 0;

	attr->permissions|=(longname.ptr[7]=='r') ? S_IROTH : 0;
	attr->permissions|=(longname.ptr[8]=='w') ? S_IWOTH : 0;
	attr->permissions|=(longname.ptr[9]=='x') ? S_IXOTH : 0;

	attr->received|=SFTP_ATTR_PERMISSIONS;

	logoutput_debug("read_attr_response_v03: perm %i", attr->permissions);

    }

    /* what to do with this ? */
    if (sftp->flags & SFTP_CLIENT_FLAG_NEWREADDIR) correction=1;

    if ((attr->received & SFTP_ATTR_USER)==0) {
	struct sftp_usermapping_s *usermapping=&sftp->usermapping;
	struct sftp_user_s user;
        char *sep=NULL;

	/* get user */

	user.remote.name.ptr=&longname.ptr[15 + correction];
	user.remote.name.len=8;
	sep=memchr(user.remote.name.ptr, ' ', 8);
	if (sep) user.remote.name.len=(unsigned int) (sep - (char *)user.remote.name.ptr);

	(* usermapping->get_local_uid)(sftp, &user);

	attr->user.uid=user.uid;
	attr->received|=SFTP_ATTR_USER;

	logoutput_debug("read_attr_response_v03: uid %i", attr->user.uid);

    }

    if ((attr->received & SFTP_ATTR_GROUP)==0) {
	struct sftp_usermapping_s *usermapping=&sftp->usermapping;
	struct sftp_group_s group;
	char *sep=NULL;

	/* get group */

	group.remote.name.ptr=&longname.ptr[24 + correction];
	group.remote.name.len=8;
	sep=memchr(group.remote.name.ptr, ' ', 8);
	if (sep) group.remote.name.len=(unsigned int) (sep - (char *)group.remote.name.ptr);

	(* usermapping->get_local_gid)(sftp, &group);

	attr->group.gid=group.gid;
	attr->received|=SFTP_ATTR_GROUP;

	logoutput("read_attr_response_v03: gid %i", attr->group.gid);

    }

    if ((attr->received & SFTP_ATTR_SIZE)==0) {

	attr->size=atoll(&longname.ptr[33 + correction]);
	attr->received|=SFTP_ATTR_SIZE;

	logoutput_debug("read_attr_response_v03: size %li", attr->size);

    }

}

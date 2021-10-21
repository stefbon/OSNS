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

#include "log.h"
#include "main.h"
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "workspace-interface.h"

#include "getent.h"

void init_getent_fields(struct getent_fields_s *fields)
{
    memset(fields, 0, sizeof(struct getent_fields_s));
    fields->flags=0;
    fields->name=NULL;
    fields->len=0;
}

void free_getent_fields(struct getent_fields_s *fields)
{
}

char *get_next_field(char *pos, int *p_size)
{
    int size=*p_size;
    char *sep=NULL;

    if (size<=0) return NULL;

    sep=memchr(pos, ':', size);

    if (sep) {

	*sep='\0';
	sep++;
	size-=(unsigned int)(sep-pos);
	*p_size=size;

    }

    return sep;
}

/* get the position of the different fields in data getent of size size, seperated by a ":"

    for a user this looks like:

    nobody:x:65534:65534:System user; nobody:/var/empty:/sbin/nologin

    for a group this looks like:

    nobody:x:65534:

*/

int get_getent_fields(struct ssh_string_s *data, struct getent_fields_s *fields)
{
    char *pos=data->ptr;
    int left=(int) data->len;
    char *tmp=NULL;

    logoutput_debug("get_getent_fields: len %i", left);

    /* name */

    fields->name=pos;
    pos=get_next_field(pos, &left);
    if (pos==NULL) goto error;
    fields->len=strlen(fields->name);

    /* ignore x/password */

    pos=get_next_field(pos, &left);
    if (pos==NULL) goto error;

    /* gid (group) or uid (user) */

    tmp=pos;
    pos=get_next_field(pos, &left);
    if (pos==NULL) goto error;

    if (fields->flags & GETENT_FLAG_GROUP) {

	fields->type.group.gid=(gid_t) atoi(tmp);
	return 0; /* ready */

    } else {

	fields->type.user.uid=(uid_t) atoi(tmp);

    }

    /* gid for user */

    tmp=pos;
    pos=get_next_field(pos, &left);
    if (pos==NULL) goto error;
    fields->type.user.uid=(uid_t) atoi(tmp);

    /* gecos (=description)*/

    fields->type.user.fullname=pos;
    pos=get_next_field(pos, &left);
    if (pos==NULL) goto error;

    /* home (==the rest)*/

    fields->type.user.home=pos;
    return 0;

    error:

    logoutput_warning("get_getent_fields: error processing %.*s", data->len, data->ptr);
    return -1;

}

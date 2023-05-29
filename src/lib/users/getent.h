/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_USERS_GETENT_H
#define LIB_USERS_GETENT_H

#include "lib/ssh/ssh-string.h"

#define GETENT_FLAG_USER				1
#define GETENT_FLAG_GROUP				2

struct getent_fields_s {
    unsigned char					flags;
    char 						*name;
    unsigned int					len;
    union {
	struct _user_s {
	    uid_t					uid;
	    gid_t					gid;
	    char 					*fullname;
	    char 					*home;
	} user;
	struct _group_s {
	    gid_t					gid;
	} group;
    } type;
};

/* prototypes */

void init_getent_fields(struct getent_fields_s *fields);
char *get_next_field(char *pos, int *p_size);
int get_getent_fields(struct ssh_string_s *data, struct getent_fields_s *fields);

#endif

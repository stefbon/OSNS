/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fsuid.h>
#include <pthread.h>

#include <pwd.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "misc.h"
#include "workspaces.h"
#include "session.h"
#include "log.h"

static struct simple_hash_s osns_users_hash;

/* functions to lookup a user using the uid */

static unsigned int calculate_uid_hash(uid_t uid)
{
    return uid % osns_users_hash.len;
}

static unsigned int uid_hashfunction(void *data)
{
    struct osns_user_s *user=(struct osns_user_s *) data;
    return calculate_uid_hash(user->pwd.pw_uid);
}

struct osns_user_s *lookup_osns_user(uid_t uid)
{
    unsigned int hashvalue=calculate_uid_hash(uid);
    void *index=NULL;
    struct osns_user_s *user=(struct osns_user_s *) get_next_hashed_value(&osns_users_hash, &index, hashvalue);

    while(user) {

	if (user->pwd.pw_uid==uid) break;
	user=(struct osns_user_s *) get_next_hashed_value(&osns_users_hash, &index, hashvalue);

    }

    return user;

}

void add_osns_user_hash(struct osns_user_s *user)
{
    add_data_to_hash(&osns_users_hash, (void *) user);
}

void remove_osns_user_hash(struct osns_user_s *user)
{
    remove_data_from_hash(&osns_users_hash, (void *) user);
}

void init_rlock_users_hash(struct simple_lock_s *l)
{
    init_rlock_hashtable(&osns_users_hash, l);
}

void init_wlock_users_hash(struct simple_lock_s *l)
{
    init_wlock_hashtable(&osns_users_hash, l);
}

void lock_users_hash(struct simple_lock_s *l)
{
    simple_lock(l);
}

void unlock_users_hash(struct simple_lock_s *l)
{
    simple_unlock(l);
}

struct osns_user_s *get_next_osns_user(void **index, unsigned int *hashvalue)
{
    struct osns_user_s *user=(struct osns_user_s *) get_next_hashed_value(&osns_users_hash, index, *hashvalue);

    while(user==NULL && *hashvalue<osns_users_hash.len) {

	(*hashvalue)++;
	user=(struct osns_user_s *) get_next_hashed_value(&osns_users_hash, index, *hashvalue);

    }

    return user;
}

static void add_list(struct osns_user_s *user, struct list_element_s *l)
{
    pthread_mutex_lock(&user->mutex);
    add_list_element_last(&user->header, l);
    pthread_mutex_unlock(&user->mutex);
}

static void remove_list(struct osns_user_s *user, struct list_element_s *l)
{
    pthread_mutex_lock(&user->mutex);
    remove_list_element(l);
    pthread_mutex_unlock(&user->mutex);
}

struct osns_user_s *add_osns_user(uid_t uid, unsigned int *error)
{
    struct osns_user_s *user=NULL;
    unsigned int size=128;

    user=lookup_osns_user(uid);

    if (user) {

	*error=EEXIST;
	return user;

    }

    allocuser:

    user=realloc(user, sizeof(struct osns_user_s) + size);

    if (user) {
	struct passwd *result=NULL;

	memset(&user->pwd, 0, sizeof(struct passwd));

	/* big enough ? */

	if (getpwuid_r(uid, &user->pwd, user->buffer, size, &result)==-1) {

	    if (errno==ERANGE) {

		size+=128;
		goto allocuser;

	    }

	    logoutput("add_osns_user: adding user uid %i failed with error %i (%s)", uid, errno, strerror(errno));
	    free(user);
	    return NULL;

	} else {

	    logoutput("add_osns_user: adding user uid %i:%s - %s at %s", uid, user->pwd.pw_name, user->pwd.pw_gecos, user->pwd.pw_dir);

	}

	user->size=size;
	user->flags=0;
	init_list_header(&user->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	pthread_mutex_init(&user->mutex, NULL);
	user->add=add_list;
	user->remove=remove_list;

	add_osns_user_hash(user);
	*error=0;

    } else {

	*error=ENOMEM;

    }

    return user;

}

void free_osns_user(void *data)
{
    if (data) {
	struct osns_user_s *user=(struct osns_user_s *) data;

	pthread_mutex_destroy(&user->mutex);
	free(user);

    }

}

int initialize_osns_users(unsigned int *error)
{
    int result=0;

    result=initialize_group(&osns_users_hash, uid_hashfunction, 32, error);

    if (result<0) {

    	logoutput("initialize_osns_users: error %i:%s initializing hashtable fuse users", *error, strerror(*error));
	return -1;

    }

    return 0;

}

void free_osns_users()
{
    free_group(&osns_users_hash, free_osns_user);
}

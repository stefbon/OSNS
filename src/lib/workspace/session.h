/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_WORKSPACE_SESSION_H
#define _LIB_WORKSPACE_SESSION_H

#include "pwd.h"
#include "list.h"

#define OSNS_USER_FLAG_INSTALL_SERVICES 			1

struct osns_user_s {
    unsigned int				flags;
    struct passwd				pwd;
    pthread_mutex_t				mutex;
    struct list_header_s			header;
    void					(* add)(struct osns_user_s *u, struct list_element_s *l);
    void					(* remove)(struct osns_user_s *u, struct list_element_s *w);
    unsigned int				size;
    char					buffer[];
};

int initialize_osns_users(unsigned int *error);
void free_osns_users();

void add_osns_user_hash(struct osns_user_s *user);
void remove_osns_user_hash(struct osns_user_s *user);

struct osns_user_s *add_osns_user(uid_t uid, unsigned int *error);
void free_osns_user(void *data);

struct osns_user_s *lookup_osns_user(uid_t uid);
struct osns_user_s *get_next_osns_user(void **index, unsigned int *hashvalue);

void init_rlock_users_hash(struct simple_lock_s *l);
void init_wlock_users_hash(struct simple_lock_s *l);

void lock_users_hash(struct simple_lock_s *l);
void unlock_users_hash(struct simple_lock_s *l);

#endif

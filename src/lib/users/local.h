/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_USERS_LOCAL_H
#define _LIB_USERS_LOCAL_H

/* prototypes */

void lock_local_userbase();
void get_local_uid_byname(char *name, uid_t *uid);
unsigned int get_local_user_byuid(uid_t uid, struct ssh_string_s *user);
void unlock_local_userbase();

void lock_local_groupbase();
void get_local_gid_byname(char *name, gid_t *gid);
unsigned int get_local_group_bygid(gid_t gid, struct ssh_string_s *group);
void unlock_local_groupbase();

#endif

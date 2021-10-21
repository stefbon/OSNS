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

#ifndef LIB_USERS_ENTITY_H
#define LIB_USERS_ENTITY_H

/* prototypes */

int compare_ent2local(struct list_element_s *list, void *b);
struct list_element_s *get_list_element_ent2local(void *b, struct sl_skiplist_s *sl);
char *get_logname_ent2local(struct list_element_s *l);

struct net_ent2local_s *get_next_ent2local(struct net_ent2local_s *ent2local);
struct net_ent2local_s *get_prev_ent2local(struct net_ent2local_s *ent2local);

struct net_ent2local_s *find_ent2local_batch(struct sl_skiplist_s *sl, struct name_s *lookupname, unsigned int *error);
struct net_ent2local_s *insert_ent2local_batch(struct sl_skiplist_s *sl , struct net_ent2local_s *ent2local, unsigned int *error);

struct net_ent2local_s *create_ent2local(struct net_entity_s *entity);
void free_ent2local(struct net_ent2local_s **p_ent2local);

#endif

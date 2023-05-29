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

#ifndef LIB_USERS_CACHE_H
#define LIB_USERS_CACHE_H

#define NET_NUMBER_REMOTE_USERS_DEFAULT			250
#define NET_NUMBER_REMOTE_GROUPS_DEFAULT		25

#define NET_DOMAIN_FLAG_ALLOC				1
#define NET_DOMAIN_FLAG_LOCALHOST			2
#define NET_DOMAIN_FLAG_NAME_ALLOC			4

struct net_domain_s {
    unsigned int					flags;
    struct list_element_s				list;	/* is one of the domains */
    struct sl_skiplist_s				*u_sl;
    struct sl_skiplist_s				*g_sl;
    struct name_s					name;
    unsigned int					size;
    char						buffer[];
};

#define NET_ENT2LOCAL_FLAG_USER				1
#define NET_ENT2LOCAL_FLAG_GROUP			2

struct net_ent2local_s {
    unsigned char					flags;
    struct list_element_s				list;
    unsigned int					remoteid;
    unsigned int					localid;
    struct name_s					name;
    unsigned int					size;
    char						buffer[];
};

#define NET_USERSCACHE_FLAG_ALLOC			1
#define NET_USERSCACHE_FLAG_INIT			2

struct net_userscache_s {
    unsigned int					flags;
    struct net_domain_s					*localhost;
    int							(* add_net2local_map)(struct net_userscache_s *cache, struct net_entity_s *entity);
    void 						(* find_user_ent2local)(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *err);
    void						(* find_group_ent2local)(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *err);
    void						(* clear)(struct net_userscache_s *c);
    unsigned int					size;
    char						buffer[];
};

/* prototypes */

struct net_userscache_s *create_net_userscache(unsigned int flags);

#endif

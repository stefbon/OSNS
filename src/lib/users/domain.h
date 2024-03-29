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

#ifndef LIB_USERS_DOMAIN_H
#define LIB_USERS_DOMAIN_H

/* prototypes */

int compare_domains(struct list_element_s *list, void *b);
struct list_element_s *get_list_element_domain(void *b, struct sl_skiplist_s *sl);
char *get_logname_domain(struct list_element_s *l);

struct net_domain_s *get_next_domain(struct net_domain_s *d);
struct net_domain_s *get_prev_domain(struct net_domain_s *d);

struct net_domain_s *find_domain_batch(struct sl_skiplist_s *sl, struct name_s *lookupname, unsigned int *error);
struct net_domain_s *insert_domain_batch(struct sl_skiplist_s *sl , struct net_domain_s *domain, unsigned int *error);

int init_net_domain(struct net_domain_s *domain, unsigned int u_size, unsigned int g_size, struct ssh_string_s *tmpname, unsigned int flags);
unsigned int get_size_net_domain(uint64_t u_count, uint64_t g_count, unsigned int *p_u_size, unsigned int *p_g_size);
struct net_domain_s *create_net_domain(struct ssh_string_s *name, unsigned int flags, uint64_t u_count, uint64_t g_count);
void free_net_domain(struct net_domain_s **domain);

#endif

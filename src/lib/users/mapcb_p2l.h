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

#ifndef LIB_USER_MAPCB_P2L_H
#define LIB_USER_MAPCB_P2L_H

/* prototypes */

void get_user_p2l_shared_byid(struct net_idmapping_s *mapping, struct net_entity_s *user);
void get_group_p2l_shared_byid(struct net_idmapping_s *mapping, struct net_entity_s *group);

void get_user_p2l_shared_byname(struct net_idmapping_s *mapping, struct net_entity_s *user);
void get_group_p2l_shared_byname(struct net_idmapping_s *mapping, struct net_entity_s *group);

void get_user_p2l_nonshared_byid(struct net_idmapping_s *mapping, struct net_entity_s *user);
void get_group_p2l_nonshared_byid(struct net_idmapping_s *mapping, struct net_entity_s *group);

void get_user_p2l_nonshared_byname(struct net_idmapping_s *mapping, struct net_entity_s *user);
void get_group_p2l_nonshared_byname(struct net_idmapping_s *mapping, struct net_entity_s *group);

#endif

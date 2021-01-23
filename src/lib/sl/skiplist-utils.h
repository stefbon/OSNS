/*
  2010, 2011, 2012, 2013 Stef Bon <stefbon@gmail.com>

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

#ifndef _SKIPLIST_UTILS_H
#define _SKIPLIST_UTILS_H

/* prototypes */

void remove_sl_dirnode(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode);
void move_sl_dirnode_left(struct sl_skiplist_s *sl, struct sl_move_dirnode_s *insert);
void move_sl_dirnode_right(struct sl_skiplist_s *sl, struct sl_move_dirnode_s *insert);

void _init_sl_dirnode(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, unsigned int size, unsigned char flag);
struct sl_dirnode_s *create_sl_dirnode(struct sl_skiplist_s *sl, unsigned short level);
unsigned short resize_sl_dirnode(struct sl_dirnode_s *dirnode, unsigned short level);
struct sl_vector_s *create_vector_path(unsigned short level, struct sl_dirnode_s *d);

void init_sl_searchresult(struct sl_searchresult_s *result, void *lookupdata, unsigned int flags);

#endif

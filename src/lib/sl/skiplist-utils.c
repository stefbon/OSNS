/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

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

#include "libosns-basic-system-headers.h"

#include "skiplist.h"
#include "libosns-log.h"

void remove_sl_dirnode(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode)
{

    for (unsigned int i=0; i<=dirnode->level; i++) {
	struct sl_dirnode_s *prev=dirnode->junction[i].p;
	struct sl_dirnode_s *next=dirnode->junction[i].n;

	prev->junction[i].step += dirnode->junction[i].step;
	prev->junction[i].n = next;
	next->junction[i].p = prev;

	sl->dirnode.junction[i].count--;

    }

}

void move_sl_dirnode_left(struct sl_skiplist_s *sl, struct sl_move_dirnode_s *move)
{
    struct sl_dirnode_s *dirnode=move->dirnode;

    // logoutput("move_sl_dirnode_left: differ %i", move->left - move->right);

    while (move->left - move->right > 1) {
	struct list_element_s *list=dirnode->list;
	struct list_element_s *prev=get_prev_element(list);

	if (prev==NULL || prev==move->list) break;
	dirnode->list=prev;
	move->step--;
	move->left--;
	move->right++;

    }

}

void move_sl_dirnode_right(struct sl_skiplist_s *sl, struct sl_move_dirnode_s *move)
{
    struct sl_dirnode_s *dirnode=move->dirnode;

    // logoutput("move_sl_dirnode_right: differ %i", move->right - move->left);

    while (move->right - move->left > 1) {
	struct list_element_s *list=dirnode->list;
	struct list_element_s *next=get_next_element(list);

	if (next==NULL || next==move->list) break;
	dirnode->list=next;
	move->step++;
	move->left++;
	move->right--;

    }

}

void _init_sl_dirnode(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, unsigned int size, unsigned char flag)
{
    unsigned short count=(size / sizeof(struct sl_junction_s));

    dirnode->list=NULL;
    dirnode->lock=0;
    dirnode->lockers=0;
    dirnode->flags|=flag;
    dirnode->level=count-1;

    for (unsigned int i=0; i<count; i++) {

	dirnode->junction[i].p=&sl->dirnode;
	dirnode->junction[i].n=&sl->dirnode;
	dirnode->junction[i].step=0;
	dirnode->junction[i].count=0;

    }

}

struct sl_dirnode_s *create_sl_dirnode(struct sl_skiplist_s *sl, unsigned short level)
{
    unsigned int size=sizeof(struct sl_dirnode_s) + (level+1) * sizeof(struct sl_junction_s);
    struct sl_dirnode_s *dirnode=malloc(size);

    if (dirnode) {

	memset(dirnode, 0, size);
	_init_sl_dirnode(sl, dirnode, size - sizeof(struct sl_dirnode_s), _DIRNODE_FLAG_ALLOC);
	dirnode->compare=sl->ops.compare;

    }

    return dirnode;

}

unsigned short resize_sl_dirnode(struct sl_dirnode_s *dirnode, unsigned short level)
{
    unsigned int size = sizeof(struct sl_dirnode_s) + (level+1) * sizeof(struct sl_junction_s);

    dirnode=(struct sl_dirnode_s *) realloc(dirnode, size);

    if (dirnode->level < level) {

	/* level increased: initialize the new headers  */

	memset(&dirnode->junction[dirnode->level], 0, (level - dirnode->level) * sizeof(struct sl_junction_s));
	dirnode->level=level;

    }

    return level;

}


struct sl_vector_s *create_vector_path(unsigned short level, struct sl_dirnode_s *dirnode)
{
    unsigned int size=sizeof(struct sl_vector_s) + (level+1) * sizeof(struct sl_path_s);
    struct sl_vector_s *vector=malloc(size);

    // logoutput("create_vector_path: level %i", level);

    if (vector) {

	memset(vector, 0, size);

	for (unsigned int i=0; i<=level; i++) {

	    vector->path[i].dirnode=dirnode;
	    vector->path[i].lock=0;
	    vector->path[i].lockset=0;
	    vector->path[i].step=0;

	}

	vector->level=level;
	vector->minlevel=0;
	vector->maxlevel=level;

    }

    return vector;

}

void init_sl_searchresult(struct sl_searchresult_s *result, void *lookupdata, unsigned int flags)
{
    result->lookupdata=lookupdata;
    result->found=NULL;
    result->ctr=0;
    result->flags=flags;
    result->row=0;
    result->step=0;
}

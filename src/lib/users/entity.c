/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#include "log.h"
#include "main.h"
#include "misc.h"

#include "threads.h"
#include "workspace-interface.h"

#include "mapping.h"

/* callbacks for users / groups */

int compare_ent2local(struct list_element_s *l, void *b)
{
    struct name_s *name=(struct name_s *) b;
    struct net_ent2local_s *ent2local=(struct net_ent2local_s *) ((char *) l - offsetof(struct net_ent2local_s, list));

    return compare_names(&ent2local->name, name);
}

struct list_element_s *get_list_element_ent2local(void *b, struct sl_skiplist_s *sl)
{
    struct name_s *name=(struct name_s *) b;
    struct net_ent2local_s *ent2local=(struct net_ent2local_s *)((char *) name - offsetof(struct net_ent2local_s, name));
    return &ent2local->list;
}

char *get_logname_ent2local(struct list_element_s *l)
{
    struct net_ent2local_s *ent2local=(struct net_ent2local_s *) ((char *) l - offsetof(struct net_ent2local_s, list));
    return ent2local->name.name;
}

struct net_ent2local_s *get_next_ent2local(struct net_ent2local_s *ent2local)
{
    struct list_element_s *next=_get_next_element(&ent2local->list);
    return (next) ? ((struct net_ent2local_s *)((char *) next - offsetof(struct net_ent2local_s, list))) : NULL;
}

struct net_ent2local_s *get_prev_ent2local(struct net_ent2local_s *ent2local)
{
    struct list_element_s *prev=_get_prev_element(&ent2local->list);
    return (prev) ? ((struct net_ent2local_s *)((char *) prev - offsetof(struct net_ent2local_s, list))) : NULL;
}

struct net_ent2local_s *find_ent2local_batch(struct sl_skiplist_s *sl, struct name_s *lookupname, unsigned int *error)
{
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) lookupname, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_find(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	*error=0;
	return (struct net_ent2local_s *)((char *) result.found - offsetof(struct net_ent2local_s, list));

    }

    *error=(result.flags & SL_SEARCHRESULT_FLAG_ERROR) ? EIO : ENOENT;
    return NULL;
}

struct net_ent2local_s *insert_ent2local_batch(struct sl_skiplist_s *sl , struct net_ent2local_s *ent2local, unsigned int *error)
{
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) &ent2local->name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_insert(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_OK) {

	*error=0;
	return (struct net_ent2local_s *)((char *) result.found - offsetof(struct net_ent2local_s, list));

    } else if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	*error=EEXIST;
	return (struct net_ent2local_s *)((char *) result.found - offsetof(struct net_ent2local_s, list));

    } else {

	*error=EIO;

    }

    return NULL;
}

struct net_ent2local_s *create_ent2local(struct net_entity_s *entity)
{
    struct net_ent2local_s *ent2local=malloc(sizeof(struct net_ent2local_s) + entity->remote.name.len);

    if (ent2local) {

	memset(ent2local, 0, sizeof(struct net_ent2local_s) + entity->remote.name.len);

	init_list_element(&ent2local->list, NULL);
	ent2local->remoteid=entity->remote.id;

	if (entity->flags & NET_ENTITY_FLAG_USER) {

	    ent2local->localid.uid=entity->local.uid;
	    ent2local->flags = NET_ENT2LOCAL_FLAG_USER;

	} else if (entity->flags & NET_ENTITY_FLAG_GROUP) {

	    ent2local->localid.gid=entity->local.gid;
	    ent2local->flags = NET_ENT2LOCAL_FLAG_GROUP;

	}

	memcpy(ent2local->buffer, entity->remote.name.ptr, entity->remote.name.len);
	ent2local->size=entity->remote.name.len;
	set_name(&ent2local->name, ent2local->buffer, ent2local->size);

    }

    return ent2local;

}

void free_ent2local(struct net_ent2local_s **p_ent2local)
{
    struct net_ent2local_s *ent2local=(p_ent2local) ? *p_ent2local : NULL;

    if (ent2local) {

	free(ent2local);
	*p_ent2local=NULL;

    }

}


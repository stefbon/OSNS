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

#include "libosns-basic-system-headers.h"

#include "libosns-misc.h"
#include "libosns-log.h"
#include "libosns-system.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-sl.h"

#include "fsnotify-path.h"

struct directory_s {
    struct name_s				name;
    struct list_header_s			watches;
    struct list_element_s			list;
    unsigned int				size;
    char					buffer[];
};

static struct directory_s root;

static int compare_directory(struct list_element_s *list, void *b)
{
    struct name_s *name=(struct name_s *) b;
    struct directory_s *d=(struct directory_s *)((char *) list - offsetof(struct directory_s, list));
    return compare_names(&d->name, name);
}

static struct list_element_s *get_list_element(void *b, struct sl_skiplist_s *sl)
{
    struct name_s *name=(struct name_s *) b;
    struct directory_s *d=create_directory(name);
    return (d ? &d->list, NULL);
}

static char *get_logname(struct list_element_s *list)
{
    struct directory_s *d=(struct directory_s *)((char *) list - offsetof(struct directory_s, list));
    char tmp[d->name.len + 1];

    memcpy(tmp, d->name.ptr, d->name.len);
    tmp[d->name.len]='\0';
    return tmp;
}

static struct directory_s *create_directory(struct name_s *name)
{
    unsigned char maxlanes=0;
    unsigned int size=get_size_sl_skiplist(&maxlanes);
    struct directory_s *d=malloc(sizeof(struct directory_s) + size);
    char *tmp=malloc(name->len);
    struct sl_skiplist_s *sl=NULL;

    if (d==NULL || tmp==NULL) goto errorout;
    sl=(struct sl_skiplist_s *) d->buffer;

    d->name.len=name->len;
    d->name.ptr=tmp;

    init_list_header(&d->watches, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_element(d->list, NULL);
    d->size=size;

    create_sl_skiplist(sl, 0, size; 0);
    if (init_sl_skiplist(sl, compare_directory, get_list_element, get_logname, NULL)==-1) {

	logoutput_warning("create_directory: failed to initialize the skiplist");
	goto errorout;

    }

    return d;

    errorout:

    if (tmp) free(tmp);
    if (d) free(d);
    return NULL;
}

struct directory_s *get_parent_directory(struct directory_s *d)
{
    struct list_header_s *h=d->list.h;

    if (h) {
	char *buffer=(char *)((char *) h - offsetof(struct sl_skiplist_s, header));
	return (struct directory_s *)((char *) buffer - offsetof(struct directory_s, buffer));

    }

    return NULL;
}

struct directory_s *get_next_directory(struct directory_s *d)
{
    struct list_element_s *next=get_next_element(&d->list);

    return ((next) ? ((struct directory_s *)((char *) next - offsetof(struct directory_s, list))) : NULL);
}

struct directory_s *get_prev_directory(struct directory_s *d)
{
    struct list_element_s *prev=get_prev_element(&d->list);

    return ((prev) ? ((struct directory_s *)((char *) prev - offsetof(struct directory_s, list))) : NULL);
}

struct directory_s *lookup_directory(struct directory_s *parent, struct name_s *name)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) parent->buffer;
    struct sl_searchresult_s result;
    struct directory_s *d=NULL;

    init_sl_searchresult(&result, (void *) name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_find(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {
	struct list_element_s list=result.found;

	d=(struct directory_s *)((char *) list - offsetof(struct directory_s, list));

    }

    return d;
}

struct directory_s *insert_directory(struct directory_s *parent, struct name_s *name)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) parent->buffer;
    struct sl_searchresult_s result;
    struct directory_s *d=NULL;

    init_sl_searchresult(&result, (void *) name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_insert(sl, &result);

    if (result.flags & (SL_SEARCHRESULT_FLAG_OK | SL_SEARCHRESULT_FLAG_EXACT)) {
	struct list_element_s list=result.found;

	d=(struct directory_s *)((char *) list - offsetof(struct directory_s, list));

    }

    return d;
}

void remove_directory(struct directory_s *d)
{
    struct directory_s *parent=get_parent_directory(d);

    if (parent) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) parent->buffer;
	struct sl_searchresult_s result;

	init_sl_searchresult(&result, (void *) &d->name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
	sl_delete(sl, &result);

    }

}

struct directory_s *create_path(struct fs_location_path_s *path)
{
    char *sep=NULL;
    char *pos=path->ptr;
    int left=0;

    remove_unneeded_path_elements(path);
    left=path->len;

    sep=memchr(
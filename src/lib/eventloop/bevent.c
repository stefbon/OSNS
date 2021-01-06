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
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>

#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <syslog.h>

#include "global-defines.h"

#include "eventloop.h"
#include "misc.h"
#include "log.h"

extern int lock_beventloop(struct beventloop_s *loop);
extern int unlock_beventloop(struct beventloop_s *loop);

static struct bevent_s *get_containing_bevent(struct list_element_s *list)
{
    return (struct bevent_s *) ( ((char *) list) - offsetof(struct bevent_s, list));
}

unsigned int set_bevent_name(struct bevent_s *bevent, char *name)
{
    unsigned int copied=0;
    unsigned int len=strlen(name);

    memset(&bevent->name, '\0', BEVENT_NAME_LEN);

    if (len < BEVENT_NAME_LEN) {

	strcpy(bevent->name, name);
	copied=len;

    } else {

	strncpy(bevent->name, name, BEVENT_NAME_LEN - 1);
	copied=BEVENT_NAME_LEN - 1;

    }

    return copied;

}

char *get_bevent_name(struct bevent_s *b)
{
    return b->name;
}

int strcmp_bevent(struct bevent_s *bevent, char *name)
{
    return strcmp(bevent->name, name);
}

static int bevent_dummy_cb(int fd, void *data, uint32_t events)
{
    return 0;
}

void init_bevent(struct bevent_s *b)
{
    unsigned int error=0;

    memset(b, 0, sizeof(struct bevent_s));

    b->fd=0;
    b->data=NULL;
    b->flags=0;
    b->cb=bevent_dummy_cb;
    init_list_element(&b->list, NULL);
    b->loop=NULL;

    set_bevent_name(b, "unknown");

}

static void add_bevent_to_list(struct bevent_s *b)
{
    struct beventloop_s *loop=b->loop;

    if ( ! loop) return;
    init_list_element(&b->list, NULL);
    add_list_element_last(&loop->bevents, &b->list);
    b->flags |= BEVENT_FLAG_LIST;

}

struct bevent_s *add_to_beventloop(int fd, uint32_t code, bevent_cb cb, void *data, struct bevent_s *bevent, struct beventloop_s *loop)
{

    logoutput("add_to_beventloop: fd %i", fd);

    if ( ! loop) loop=get_mainloop();
    lock_beventloop(loop);

    if ( ! bevent ) {

	bevent=malloc(sizeof(struct bevent_s));

	if (bevent==NULL) {

	    logoutput("add_to_beventloop: create bevent failed");
	    goto unlock;

	}

	init_bevent(bevent);
	bevent->flags|=BEVENT_FLAG_ALLOCATED;

    }

    bevent->fd=fd;

    if ((* loop->add_bevent)(loop, bevent, code)==0) {

	logoutput("add_to_beventloop: added fd %i", fd);
	bevent->cb=cb;
	bevent->data=data;

    } else {

	logoutput("add_to_beventloop: error adding fd %i", fd);
	bevent->fd=-1;
	goto unlock;

    }

    add_bevent_to_list(bevent);

    unlock:

    unlock_beventloop(loop);
    return bevent;

}

void remove_bevent_from_beventloop(struct bevent_s *bevent)
{
    struct beventloop_s *loop=NULL;

    if (bevent==NULL || bevent->loop==NULL) return;
    loop=bevent->loop;
    lock_beventloop(loop);

    if (bevent->flags & BEVENT_FLAG_EVENTLOOP) (* loop->remove_bevent)(bevent);

    if (bevent->flags & BEVENT_FLAG_LIST) {

	remove_list_element(&bevent->list);
	bevent->flags-=BEVENT_FLAG_LIST;

    }

    unlock_beventloop(loop);
}

void modify_bevent(struct bevent_s *bevent, uint32_t code)
{
    struct beventloop_s *loop=NULL;

    if (bevent==NULL || bevent->loop==NULL) return;
    loop=bevent->loop;
    lock_beventloop(loop);
    if (bevent->flags & BEVENT_FLAG_EVENTLOOP) (* loop->modify_bevent)(bevent, code);
    unlock_beventloop(loop);
}

struct bevent_s *get_next_bevent(struct beventloop_s *loop, struct bevent_s *bevent)
{
    struct list_element_s *list=NULL;

    if (bevent) {

	list=get_next_element(&bevent->list);

    } else {

	if ( ! loop) loop=get_mainloop();
	list=get_list_head(&loop->bevents, 0);

    }

    return (list) ? get_containing_bevent(list) : NULL;

}

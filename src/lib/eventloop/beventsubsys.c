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

#include <time.h>
#include <sys/wait.h>

#include "libosns-misc.h"
#include "libosns-log.h"

#include "beventloop.h"
#include "bevent.h"

static int _subsys_dummy_start(struct bevent_subsystem_s *subsys)
{
    return -1;
}
static void _subsys_dummy_action(struct bevent_subsystem_s *subsys)
{
}

static struct bevent_subsystem_ops_s dummy_ops = {
    .start_subsys			= _subsys_dummy_start,
    .stop_subsys			= _subsys_dummy_action,
    .clear_subsys			= _subsys_dummy_action,
};

static struct bevent_subsystem_s dummy_subsys={
    .flags					= BEVENT_SUBSYSTEM_FLAG_DUMMY,
    .name					= "dummy",
    .ops					= &dummy_ops,
    .size					= 0,
};

struct bevent_subsystem_s *get_dummy_bevent_subsystem()
{
    return &dummy_subsys;
}

struct bevent_subsystem_s *create_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int size)
{
    struct bevent_subsystem_s *subsys=malloc(sizeof(struct bevent_subsystem_s) + size);

    if (subsys) {

	memset(subsys, 0, sizeof(struct bevent_subsystem_s) + size);
	subsys->flags = BEVENT_SUBSYSTEM_FLAG_ALLOC;
	subsys->size=size;
	subsys->ops=&dummy_ops;

    }

    return subsys;

}

unsigned int complete_bevent_subsystem_common(struct beventloop_s *eloop, struct bevent_subsystem_s *subsys)
{
    unsigned int id=0;

    if (eloop==NULL) eloop=get_default_mainloop();

    /* add to array of subsystems */
    signal_lock_flag(eloop->signal, &eloop->flags, BEVENTLOOP_FLAG_SUBSYSTEMS_LOCK);

    eloop->asubsystems[eloop->count]=subsys;
    id=eloop->count;
    eloop->count++;
    signal_unlock_flag(eloop->signal, &eloop->flags, BEVENTLOOP_FLAG_SUBSYSTEMS_LOCK);
    return id;
}

int start_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name)
{

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	return ((strcmp(subsys->type_name, type_name)==0) ? (* subsys->ops->start_subsys)(subsys) : -1);

    }

    return -1;

}

void stop_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name)
{

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	if (strcmp(subsys->type_name, type_name)==0) (* subsys->ops->stop_subsys)(subsys);

    }

}

void clear_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name)
{

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	if (strcmp(subsys->type_name, type_name)==0) (* subsys->ops->clear_subsys)(subsys);

    }

}

void free_bevent_subsystem_common(struct beventloop_s *eloop, unsigned int id, const char *type_name)
{

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	if (((subsys->flags & BEVENT_SUBSYSTEM_FLAG_DUMMY)==0) && strcmp(subsys->type_name, type_name)==0) {

	    if ((subsys->flags & BEVENT_SUBSYSTEM_FLAG_STOP)==0) (* subsys->ops->stop_subsys)(subsys);
	    if ((subsys->flags & BEVENT_SUBSYSTEM_FLAG_CLEAR)==0) (* subsys->ops->clear_subsys)(subsys);

	    eloop->asubsystems[id]=&dummy_subsys; /* replace the subsys by a dummy one to prevent segfaults ... */
	    free(subsys);

	}

    }

}

int find_bevent_subsystem(struct beventloop_s *eloop, const char *type_name)
{
    struct bevent_subsystem_s *subsys=NULL;
    int id=-1;

    logoutput_debug("find_bevent_subsystem: look for %s", type_name);

    if (eloop==NULL) eloop=get_default_mainloop();

    for (unsigned int i=0; i<eloop->count; i++) {

	subsys=eloop->asubsystems[i];

	logoutput_debug("find_bevent_subsystem: found subsys for %s", ((subsys->type_name) ? subsys->type_name : "NONAME"));

	if (subsys && ((subsys->flags & BEVENT_SUBSYSTEM_FLAG_DUMMY)==0) && strcmp(subsys->type_name, type_name)==0) {

	    id=i;
	    break;

	}

    }

    return id;
}

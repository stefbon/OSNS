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
#include "beventsubsys.h"

/* static default mainloop */
static struct beventloop_s beventloop_main;
static unsigned char init_done=0;
static pthread_mutex_t init_mutex=PTHREAD_MUTEX_INITIALIZER;
static int result_init_keep=-1;

static pthread_mutex_t loop_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t loop_cond=PTHREAD_COND_INITIALIZER;

void _loop_dummy_action(struct beventloop_s *l)
{}

static struct bevent_s *create_bevent_dummy(struct beventloop_s *l)
{
    return NULL;
}
static int init_io_bevent_dummy(struct beventloop_s *l, struct bevent_s *b)
{
    return -1;
}

static struct beventloop_ops_s dummy_init_ops = {
    .start_eventloop			= _loop_dummy_action,
    .stop_eventloop			= _loop_dummy_action,
    .clear_eventloop			= _loop_dummy_action,
    .create_bevent			= create_bevent_dummy,
    .init_io_bevent			= init_io_bevent_dummy,
};

static void first_run_ignore(struct beventloop_s *eloop, void *ptr)
{
}

static void start_first_run_ignore(struct beventloop_s *loop)
{
}

static void start_first_run_cb(struct beventloop_s *loop)
{

    logoutput_debug("start_first_run_cb: loop started");

    signal_set_flag(loop->signal, &loop->flags, BEVENTLOOP_FLAG_START);
    (* loop->first_run_ctx_cb)(loop, loop->ptr);
    loop->first_run=start_first_run_ignore;
}

static int _init_beventloop(struct beventloop_s *eloop)
{
    struct bevent_subsystem_s *dummy_subsys=get_dummy_bevent_subsystem();
    unsigned int id=0;

    logoutput("_init_beventloop");

    init_list_header(&eloop->bevents, SIMPLE_LIST_TYPE_EMPTY, NULL);
    eloop->signal=get_custom_shared_signal(&loop_mutex, &loop_cond);
    if (eloop->signal==NULL) eloop->signal=get_default_shared_signal();

    eloop->ptr=NULL;
    eloop->first_run=start_first_run_cb;
    eloop->first_run_ctx_cb=first_run_ignore;

    eloop->count=0;
    for (unsigned int i=0; i<BEVENTLOOP_MAX_SUBSYSTEMS; i++) eloop->asubsystems[i]=NULL;
    id=complete_bevent_subsystem_common(eloop, dummy_subsys);
    logoutput("_init_beventloop: added dummy subsys with id %i", id);

#ifdef USE_LINUX_EPOLL

    eloop->flags |= BEVENTLOOP_FLAG_EPOLL;

    if (init_beventloop_epoll(eloop)==-1) {

	logoutput_warning("_init_beventloop: unable to initialize backend epoll eventloop");
	goto errorfailed;

    }

    logoutput("_init_beventloop: backend initialized");

#else

    eloop->flags |= BEVENTLOOP_FLAG_GLIB;

    if (init_beventloop_glib(eloop)==-1) {

	logoutput_warning("_init_beventloop: unable to initialize backend glib eventloop");
	goto errorfailed;

    }

    logoutput("_init_beventloop: backend initialized");


#endif

    eloop->flags |= BEVENTLOOP_FLAG_INIT;
    return 0;

    errorfailed:
    clear_shared_signal(&eloop->signal);
    return -1;

}

struct beventloop_s *create_beventloop()
{
    struct beventloop_s *eloop=malloc(sizeof(struct beventloop_s));

    if (eloop) {

	memset(eloop, 0, sizeof(struct beventloop_s));
	eloop->ops=&dummy_init_ops;
	eloop->flags=BEVENTLOOP_FLAG_ALLOC;

    }

    return eloop;
}

int init_beventloop(struct beventloop_s *eloop)
{
    int result=-1;

    logoutput("init_beventloop");

    if (eloop) {

	return _init_beventloop(eloop);

    } else {

	eloop=&beventloop_main;

    }

    pthread_mutex_lock(&init_mutex);

    if (init_done==0) {

	memset(eloop, 0, sizeof(struct beventloop_s));
	eloop->flags |= BEVENTLOOP_FLAG_MAIN;
	eloop->ops=&dummy_init_ops;

	result=_init_beventloop(eloop);
	init_done=1;
	result_init_keep=result;

    } else {

	result=result_init_keep;

    }

    pthread_mutex_unlock(&init_mutex);
    return result;
}

void set_first_run_beventloop(struct beventloop_s *eloop, void (* cb)(struct beventloop_s *eloop, void *ptr), void *ptr)
{
    eloop->first_run_ctx_cb=cb;
    eloop->ptr=ptr;
}

void start_beventloop(struct beventloop_s *eloop)
{
    if (eloop==NULL) eloop=&beventloop_main;
    (* eloop->ops->start_eventloop)(eloop);
}

void stop_beventloop(struct beventloop_s *eloop)
{
    if (!eloop) eloop=&beventloop_main;
    (* eloop->ops->stop_eventloop)(eloop);
}

void clear_beventloop(struct beventloop_s *eloop)
{
    struct list_element_s *list;

    if (!eloop) eloop=&beventloop_main;

    /* backend of the eventloop */
    (* eloop->ops->clear_eventloop)(eloop);

    /* eventloop subsystems */

    /* for (unsigned int i=0; i<eloop->count; i++) {

	struct bevent_subsystem_s *subsys=eloop->asubsystems[i];

	if ((subsys->flags & BEVENT_SUBSYSTEM_FLAG_DUMMY)==0) {

	    logoutput_debug("clear_beventloop: found subsystem %s", (strlen(subsys->name)>0 ? subsys->name : "UNKNOWN"));

	    (* subsys->ops->stop_subsys)(subsys);
	    (* subsys->ops->clear_subsys)(subsys);
	    eloop->asubsystems[i]=NULL;
	    free(subsys);

	}

    } *.

    /* bevents */

    /* list=get_list_head(&eloop->bevents, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct bevent_s *bevent=(struct bevent_s *)((char *) list - offsetof(struct bevent_s, list));

	remove_bevent_watch(bevent, 0);
	free_bevent(&bevent);
	list=get_list_head(&eloop->bevents, SIMPLE_LIST_FLAG_REMOVE);

    } */

    clear_shared_signal(&eloop->signal);

}

void free_beventloop(struct beventloop_s **p_loop)
{
    struct beventloop_s *eloop=*p_loop;

    if (eloop) {

	clear_beventloop(eloop);

	if (eloop->flags & BEVENTLOOP_FLAG_ALLOC) {

	    free(eloop);
	    *p_loop=NULL;

	}

    }

}

struct beventloop_s *get_default_mainloop()
{
    return &beventloop_main;
}

void set_type_beventloop(struct beventloop_s *loop, unsigned int flag)
{

    if (loop) {
	unsigned int alltypes=(BEVENTLOOP_FLAG_EPOLL | BEVENTLOOP_FLAG_GLIB);

	if (flag & alltypes) {

	    loop->flags &= ~alltypes; /* whipe out any previous type */
	    loop->flags |= flag;

	}

    }

}


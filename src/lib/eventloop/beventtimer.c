/*
  2021 Stef Bon <stefbon@gmail.com>

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
#include "libosns-system.h"

#include "beventloop.h"
#include "bevent.h"
#include "beventsubsys.h"

#ifdef __linux__
#include "backend/linux-timerfd.h"
#endif

static const char *type_name="timer";

static int init_bevent_subsys_timer(struct beventloop_s *eloop, struct bevent_subsystem_s *subsys)
{

#ifdef __linux__

    return init_timerfd_backend(eloop, subsys);

#else

    return ((subsys) ? -1 : 0);

#endif

}

int create_bevent_timer_subsystem(struct beventloop_s *eloop)
{
    unsigned int size=0;
    struct bevent_subsystem_s *subsys=NULL;
    int result=-1;

    if (eloop==NULL) eloop=get_default_mainloop();

    /* test for max ... maybe a dynamic array but 16 sub systems is alreay a lot */

    if (eloop->count==16) {

	logoutput_warning("create_bevent_timer_subsystem: unable to create subsystem, maximum count %i reached", eloop->count);
	return -1;

    }

    size=init_bevent_subsys_timer(eloop, NULL);
    subsys=create_bevent_subsystem_common(eloop, size);

    if (subsys) {

	if (init_bevent_subsys_timer(eloop, subsys)==0) {

	    subsys->type_name=type_name;
	    result=(int) complete_bevent_subsystem_common(eloop, subsys);

	} else {

	    logoutput_warning("create_bevent_timer_subsystem: unable to create timer subsystem");
	    goto failed;

	}

    } else {

	logoutput_warning("create_bevent_timer_subsystem: unable to create subsystem");
	goto failed;

    }

    return result;

    failed:

    if (subsys && subsys->flags & BEVENT_SUBSYSTEM_FLAG_ALLOC) free(subsys);
    return -1;

}

int start_btimer_subsystem(struct beventloop_s *eloop, unsigned int id)
{
    return start_bevent_subsystem_common(eloop, id, type_name);
}

void stop_btimer_subsystem(struct beventloop_s *eloop, unsigned int id)
{
    stop_bevent_subsystem_common(eloop, id, type_name);
}

void clear_btimer_subsystem(struct beventloop_s *eloop, unsigned int id)
{
    clear_bevent_subsystem_common(eloop, id, type_name);
}

unsigned int add_btimer_eventloop(struct beventloop_s *eloop, unsigned int id, struct system_timespec_s *expire, void (* cb)(unsigned int id, void *ptr, unsigned char flags), void *ptr)
{

    logoutput_debug("add_btimer_eventloop");

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	if (subsys && strcmp(subsys->type_name, type_name)==0) return add_timer_timerfd(subsys, expire, cb, ptr);

    }

    return 0;
}

unsigned char modify_timer_eventloop(struct beventloop_s *eloop, unsigned int id, unsigned int timerid, struct system_timespec_s *expire)
{

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	if (strcmp(subsys->type_name, type_name)==0) return modify_timer_timerfd(subsys, timerid, NULL, expire);

    }

    return 0;
}

unsigned char remove_timer_eventloop(struct beventloop_s *eloop, unsigned int id, unsigned int timerid)
{

    if (eloop==NULL) eloop=get_default_mainloop();

    if (id < eloop->count) {
	struct bevent_subsystem_s *subsys=eloop->asubsystems[id];

	if (strcmp(subsys->type_name, type_name)==0) return remove_timer_timerfd(subsys, timerid, NULL);

    }

    return 0;
}

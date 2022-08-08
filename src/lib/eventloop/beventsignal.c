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

#include "beventloop.h"
#include "bevent.h"
#include "beventsubsys.h"

#ifdef __linux__
#include "backend/linux-signalfd.h"
#endif

static const char *type_name="signal";

static int init_bevent_subsys_signal(struct beventloop_s *eloop, struct bevent_subsystem_s *subsys)
{

#ifdef __linux__

    return init_signalfd_subsystem(eloop, subsys);

#else

    return ((subsys) ? -1 : 0);

#endif

}

int create_bevent_signal_subsystem(struct beventloop_s *eloop, void (* cb)(struct beventloop_s *eloop, struct bsignal_event_s *bse))
{
    unsigned int size=0;
    struct bevent_subsystem_s *subsys=NULL;
    int result=-1;

    if (eloop==NULL) eloop=get_default_mainloop();
    size=init_bevent_subsys_signal(eloop, NULL);

    /* test for max ... maybe a dynamic array but 16 sub systems is alreay a lot */

    if (eloop->count==BEVENTLOOP_MAX_SUBSYSTEMS) {

	logoutput_warning("create_bevent_signal_subsystem: unable to create subsystem, maximum count %i reached", eloop->count);
	return -1;

    }

    subsys=create_bevent_subsystem_common(eloop, size);

    if (subsys) {

	if (init_bevent_subsys_signal(eloop, subsys)==0) {

	    subsys->type_name=type_name;
	    set_cb_signalfd_subsystem(subsys, cb);
	    result=(int) complete_bevent_subsystem_common(eloop, subsys);

	} else {

	    logoutput_warning("create_bevent_signal_subsystem: unable to create timer subsystem");
	    goto failed;

	}

    } else {

	logoutput_warning("create_bevent_signal_subsystem: unable to create subsystem");
	goto failed;

    }

    logoutput("create_bevent_signal_subsystem: created subsystem");

    return result;

    failed:

    if (subsys && subsys->flags & BEVENT_SUBSYSTEM_FLAG_ALLOC) free(subsys);
    return -1;

}

int start_bsignal_subsystem(struct beventloop_s *eloop, unsigned int id)
{
    return start_bevent_subsystem_common(eloop, id, type_name);
}

void stop_bsignal_subsystem(struct beventloop_s *eloop, unsigned int id)
{
    stop_bevent_subsystem_common(eloop, id, type_name);
}

void clear_bsignal_subsystem(struct beventloop_s *eloop, unsigned int id)
{
    clear_bevent_subsystem_common(eloop, id, type_name);
}

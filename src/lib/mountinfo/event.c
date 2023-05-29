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

#include <sys/sysmacros.h>
#include <fcntl.h>

#include "libosns-log.h"
#include "libosns-list.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"

#include "monitor.h"
#include "list.h"
#include "read.h"

void handle_change_mounttable(struct mount_monitor_s *monitor)
{

    /* get a new list and compare */

    monitor->generation++;
    logoutput("handle_change_mounttable: generation %lu", monitor->generation);

    if (get_mountinfo_list(monitor, MOUNT_MONITOR_FLAG_IGNORE_PSEUDOFS)==0) {

	process_mountentries(monitor);

    }

}

static void thread_process_mountinfo(void *ptr)
{
    struct mount_monitor_s *monitor=(struct mount_monitor_s *) ptr;

    logoutput("thread_process_mountinfo");

    signal_lock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);

    if (monitor->status & MOUNT_MONITOR_STATUS_PROCESS) {

        monitor->status |= MOUNT_MONITOR_STATUS_CHANGED;
        signal_unlock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);
        return;

    }

    monitor->status |= MOUNT_MONITOR_STATUS_PROCESS;

    processmounttable:

    monitor->status &= ~MOUNT_MONITOR_STATUS_CHANGED;
    signal_unlock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);

    handle_change_mounttable(monitor);

    signal_lock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);
    if (monitor->status & MOUNT_MONITOR_STATUS_CHANGED) goto processmounttable;
    monitor->status &= ~MOUNT_MONITOR_STATUS_PROCESS;
    signal_unlock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);

}

/* process an event the mountinfo */

void process_mountinfo_event(struct osns_socket_s *sock, unsigned int level, unsigned int errcode, void *ptr)
{
    struct mount_monitor_s *monitor=(struct mount_monitor_s *) ptr;

    logoutput_debug("process_mountinfo_event");

    signal_lock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);

    if (monitor->status & MOUNT_MONITOR_STATUS_PROCESS) {

	monitor->status |= MOUNT_MONITOR_STATUS_CHANGED;

    } else {

	/* get a thread to do the work */

	work_workerthread(NULL, 0, thread_process_mountinfo, (void *) monitor);

    }

    signal_unlock_flag(monitor->signal, &monitor->status, MOUNT_MONITOR_STATUS_MOUNTEVENT);

}

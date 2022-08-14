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
#include "read.h"
#include "list.h"
#include "event.h"

#ifdef __linux__
#define MOUNTINFO_FILE "/proc/self/mountinfo"
#endif

static struct mount_monitor_s monitor;

/* dummy callbacks */

static unsigned char dummy_ignore(char *source, char *fs, char *mountpoint, void *data)
{
    unsigned char doignore=0; /* by default ignore nothing */

#ifdef __linux__

    /* on Linux ignore the system mountpoints/filesystems */

    if ((strlen(mountpoint) >= 6) && memcmp(mountpoint, "/proc/", 6)==0) {

	doignore=1;

    } else if ((strlen(mountpoint) >= 5) && (memcmp(mountpoint, "/sys/", 5)==0 || memcmp(mountpoint, "/dev/", 5)==0)) {

	doignore=1;

    }

#endif

    return doignore;
}

static int dummy_update(unsigned char what, struct mountentry_s *me)
{
    logoutput_debug("dummy_update: %s %s:%s at %s (gen=%lu)", ((what==MOUNTMONITOR_ACTION_ADDED) ? "added" : "removed"), me->source, me->fs, me->mountpoint, me->generation);
    return 1;
}

static int open_mountmonior_system_watch(struct osns_socket_s *sock)
{

#ifdef __linux__

    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;

    set_location_path(&path, 'c', MOUNTINFO_FILE);
    logoutput_debug("open_mountmonior_system_watch: open file %s", MOUNTINFO_FILE);
    init_osns_socket(sock, OSNS_SOCKET_TYPE_FILESYSTEM, OSNS_SOCKET_FLAG_FILE, &path);

#endif

    return ((* sock->sops.filesystem.file.open)(NULL, &path, sock, NULL, 0);

}

FILE *fopen_mountmonitor()
{
#ifdef __linux__
    return fopen(MOUNTINFO_FILE, "r");
#else
    return NULL;
#endif
}

static void close_mountmonitor_system_watch(struct osns_socket_s *sock)
{
    (* sock->sops.close)(sock);

    if (sock->event.type==SOCKET_EVENT_TYPE_BEVENT) {
	struct bevent_s *bevent=sock->event.link.bevent;

	if (bevent) {

	    remove_bevent_watch(bevent, 0);
	    free_bevent(&bevent);

	    sock->event.type=0;
	    sock->event.link.bevent=NULL;

	}

    }

}

struct bevent_s *open_mountmonitor(struct shared_signal_s *signal, void *ptr, unsigned int flags)
{
    struct bevent_s *bevent=NULL;

    logoutput_debug("open_mountmonitor");

    monitor.status=MOUNT_MONITOR_STATUS_INIT;
    monitor.flags=(flags & MOUNT_MONITOR_FLAG_IGNORE_PSEUDOFS);
    monitor.generation=0;
    monitor.signal=signal;

    monitor.update=dummy_update;
    monitor.ignore=dummy_ignore;
    monitor.data=ptr;

    init_list_header(&monitor.mountentries, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&monitor.removedentries, SIMPLE_LIST_TYPE_EMPTY, NULL);

    monitor.size=MOUNT_MONITOR_DEFAULT_BUFFER_SIZE;
    monitor.buffer=malloc(monitor.size);
    if (monitor.buffer==NULL) {

	logoutput_error("open_mountmonitor: unable to allocate %u bytes", monitor.size);
	goto error;

    }

    if (open_mountmonior_system_watch(&monitor.sock)==-1) {

	logoutput_error("open_mountmonitor: unable to open the system watch");
	goto error;

    }

    bevent=create_fd_bevent(NULL, (void *) &monitor);
    if (bevent==NULL) goto error;
    set_bevent_osns_socket(bevent, &monitor.sock);
    set_bevent_cb(bevent, (BEVENT_FLAG_CB_PRI | BEVENT_FLAG_CB_ERROR), process_mountinfo_event);
    return bevent;

    error:
    close_mountmonitor_system_watch(&monitor.sock);
    return NULL;
}

void read_mounttable()
{
    handle_change_mounttable(&monitor);
}

void set_mount_monitor_ignore_cb(struct mount_monitor_s *m, unsigned char (* ignore_cb) (char *source, char *fs, char *path, void *data))
{
    if (m==NULL) m=&monitor;
    m->ignore=((ignore_cb) ? ignore_cb : dummy_ignore);
}

void set_mount_monitor_update_cb(struct mount_monitor_s *m, int (* update_cb) (unsigned char what, struct mountentry_s *me))
{
    if (m==NULL) m=&monitor;
    m->update=((update_cb) ? update_cb : dummy_update);
}

void close_mountmonitor()
{

    close_mountmonitor_system_watch(&monitor.sock);

    if (monitor.buffer) {

	free(monitor.buffer);
	monitor.buffer=NULL;

    }

    clear_mountentry_lists(&monitor);

}

struct mount_monitor_s *get_default_mount_monitor()
{
    return &monitor;
}

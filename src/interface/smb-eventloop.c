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
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/poll.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "users.h"

#include "smb.h"
#include "smb-signal.h"

extern struct beventloop_s *get_workspace_eventloop(struct context_interface_s *i);

short translate_bevent_to_poll(struct event_s *event)
{
    short revents=0;

    /* translate the signal from osns into something smb2 understands
	is it possible to do this more effecient ? */

    revents |= (signal_is_data(event) ? (POLLIN | POLLPRI) : 0);
    revents |= (signal_is_error(event) ? POLLERR : 0);
    revents |= (signal_is_buffer(event) ? POLLOUT : 0);
    revents |= (signal_is_close(event) ? POLLHUP : 0);

    return revents;

}

void translate_poll_to_bevent(struct bevent_s *bevent, short events)
{

    if (events & POLLIN) {

	set_bevent_watch(bevent, "i");
	set_bevent_watch(bevent, "u");

    }

    if (events & POLLOUT) {

	set_bevent_watch(bevent, "o");

    }

}

void process_smb_share_event(int fd, void *ptr, struct event_s *event)
{
    short revents=translate_bevent_to_poll(event);

    logoutput("process_smb_share_event: revents %i", revents);

    if (revents) {
	struct context_interface_s *interface=(struct context_interface_s *) ptr;
	struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
	struct smb2_context *smb2=(struct smb2_context *) share->ptr;

	smb2_service(smb2, revents);

    }

}

int wait_smb_share_connected(struct context_interface_s *interface, struct timespec *timeout)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct smb_signal_s *signal=&share->signal;
    struct timespec expire;
    int result=0;

    logoutput("wait_smb_share_connected");

    get_expire_time(&expire, timeout);

    smb_signal_lock(signal);

    startcheckwait:

    logoutput("wait_smb_share_connected: flags %i", share->flags);

    if (share->flags & _SMB_SHARE_FLAG_CONNECTED) {

	smb_signal_unlock(signal);
	return 1;

    } else if (share->flags & _SMB_SHARE_FLAG_ERROR) {

	smb_signal_unlock(signal);
	return 0;

    }

    logoutput("wait_smb_share_connected: C");

    result=smb_signal_condtimedwait(signal, &expire);

    logoutput("wait_smb_share_connected: D result %i", result);

    if (result>0) {

	smb_signal_unlock(signal);
	share->error=result;
	return 0;

    } else if ((share->flags & _SMB_SHARE_FLAG_CONNECTED)==0) {

	goto startcheckwait;

    }

    logoutput("wait_smb_share_connected: E");

    result=1;
    smb_signal_unlock(signal);

    out:
    return result;

}

void _smb2_change_fd_cb(struct smb2_context *smb2, int fd, int cmd)
{
    struct context_interface_s *interface=(struct context_interface_s *) smb2_get_opaque(smb2);
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct beventloop_s *loop=get_workspace_eventloop(interface);

    logoutput("_smb2_change_fd_cb: fd %i cmd %i:%s loop %s", fd, cmd, ((cmd==SMB2_ADD_FD) ? "add" : "rm"), ((loop) ? "def" : "null"));

    if (cmd==SMB2_ADD_FD) {
	struct bevent_s *bevent=NULL;

	bevent=create_fd_bevent(loop, process_smb_share_event, (void *) interface);
	if (bevent) {

	    set_bevent_unix_fd(bevent, fd);
	    share->bevent=bevent;

	}

    } else if (cmd==SMB2_DEL_FD) {
	struct bevent_s *bevent=share->bevent;

	if (bevent) {

	    if (get_bevent_unix_fd(bevent)!=fd) {

		logoutput_warning("_smb2_change_fd_cb: internal error ... removing fd %i while fd %i is used for connection", fd, get_bevent_unix_fd(bevent));

	    } else {

		logoutput("_smb2_change_fd_cb: remove_bevent");
		remove_bevent(bevent);
		logoutput("_smb2_change_fd_cb: free_bevent");
		free_bevent(&bevent);
		share->bevent=NULL;

	    }

	} else {

	    logoutput("_smb2_change_fd_cb: bevent NULL");

	}

    }

}

void _smb2_change_events_cb(struct smb2_context *smb2, int fd, int events)
{
    struct context_interface_s *interface=(struct context_interface_s *) smb2_get_opaque(smb2);
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct beventloop_s *loop=get_workspace_eventloop(interface);
    struct bevent_s *bevent=NULL;

    logoutput("_smb2_change_events_cb: fd %i loop %s", fd, ((loop) ? "def" : "null"));

    bevent=share->bevent;

    if (get_bevent_unix_fd(bevent)!=fd) {

	logoutput_warning("_smb2_change_events_cb: internal error ... change fd %i while fd %i is used for connection", fd, get_bevent_unix_fd(bevent));

    } else {

	logoutput("_smb2_change_events_cb: events %i", events);

	if (get_bevent_events(bevent) != (short) events) {

	    modify_bevent_watch(bevent, NULL, events);

	}

    }

}


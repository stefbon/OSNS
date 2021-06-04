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
#include <sys/signalfd.h>

#include "misc.h"
#include "log.h"
#include "beventloop.h"
#include "beventsignal.h"

static struct bevent_s *signal_bevent=NULL;
static void (* signal_cb)(struct beventloop_s *loop, unsigned int signo, pid_t pid, int fd);

static void default_signal_cb(struct beventloop_s *loop, unsigned int signo, pid_t pid, int fd)
{

    if ( signo==SIGHUP || signo==SIGINT || signo==SIGTERM ) {

	logoutput("default_signal_cb: caught signal %i sender %i", signo, (unsigned int) pid);
	stop_beventloop(loop);

    } else {

	if (signo==EIO) {

	    logoutput("default_signal_cb: caught signal %i sender %i fd %i", signo, (unsigned int) pid, fd);

	} else {

	    logoutput("default_signal_cb: caught signal %i sender %i", signo, (unsigned int) pid);

	}

    }

}

static void read_signal_eventloop(int fd, void *ptr, struct event_s *events)
{

    if (signal_is_data(events)) {
	struct signalfd_siginfo fdsi;
	ssize_t len=0;

	len=read(fd, &fdsi, sizeof(struct signalfd_siginfo));

	if (len == sizeof(struct signalfd_siginfo)) {
	    struct beventloop_s *loop=(struct beventloop_s *) ptr;

	    (* signal_cb) (loop, fdsi.ssi_signo, fdsi.ssi_pid, fdsi.ssi_fd);

	}

    }

}

void disable_beventloop_signal(struct beventloop_s *loop)
{

    if (signal_bevent) {
	int fd=get_bevent_unix_fd(signal_bevent);

	if (fd>=0) {

	    close(fd);
	    set_bevent_unix_fd(signal_bevent, -1);

	}

	remove_bevent(signal_bevent);
	signal_bevent=NULL;
	loop->flags &= ~BEVENTLOOP_FLAG_SIGNAL;


    }

}

static int add_signalhandler(struct beventloop_s *loop, void (* cb) (struct beventloop_s *loop, unsigned int signo, pid_t pid, int fd))
{
    sigset_t sigset;
    int fd=-1;

    if (loop->flags & BEVENTLOOP_FLAG_SIGNAL || signal_bevent) return 0;
    signal_cb=(cb) ? cb : default_signal_cb;

    sigemptyset(&sigset);

    /* default set of signals to react on -> configurable? */

    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGSTOP);
    sigaddset(&sigset, SIGABRT);

    /* io event on socket */

    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGPIPE);

    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) goto error;

    fd = signalfd(-1, &sigset, 0);
    if (fd == -1) {

	logoutput_warning("add_signalhandler: error %i open signalfd (%s)", errno, strerror(errno));
	goto error;

    } else {

	logoutput_debug("add_signalhandler: open signalfd %i", fd);

    }

    signal_bevent=create_fd_bevent(NULL, read_signal_eventloop, (void *) loop);
    if (signal_bevent==NULL) {

	logoutput_warning("add_signalhandler: error creating bevent");
	goto error;

    }

    set_bevent_unix_fd(signal_bevent, fd);
    set_bevent_watch(signal_bevent, "incoming data");

    if (add_bevent_beventloop(signal_bevent)==0) {

	logoutput_debug("add_signalhandler: added signal to eventloop");
	return 0;

    } else {

	logoutput_warning("add_signalhandler: error adding bevent");

    }

    error:

    disable_beventloop_signal(loop);
    logoutput("add_signalhandler: error");
    return -1;

}

int enable_beventloop_signal(struct beventloop_s *loop, void (* cb) (struct beventloop_s *loop, unsigned int signo, pid_t pid, int fd))
{
    logoutput_debug("enable_beventloop_signal");

    if (! loop) loop=get_mainloop();
    return add_signalhandler(loop, cb);
}


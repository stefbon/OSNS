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

#include "misc.h"
#include "log.h"
#include "beventloop.h"
#include "bevent.h"

extern int lock_beventloop(struct beventloop_s *loop);
extern int unlock_beventloop(struct beventloop_s *loop);

static void default_signal_cb(struct beventloop_s *loop, void *data, struct signalfd_siginfo *fdsi)
{
    unsigned int signo=fdsi->ssi_signo;

    if ( signo==SIGHUP || signo==SIGINT || signo==SIGTERM ) {

	logoutput("default_signal_cb: caught signal %i sender %i", signo, (unsigned int) fdsi->ssi_pid);
	loop->status=BEVENTLOOP_STATUS_DOWN;

    } else {

	if (signo==EIO) {

	    logoutput("default_signal_cb: caught signal %i sender %i fd %i", signo, (unsigned int) fdsi->ssi_pid, fdsi->ssi_fd);

	} else {

	    logoutput("default_signal_cb: caught signal %i sender %i", signo, (unsigned int) fdsi->ssi_pid);

	}

    }

}

static int read_signal_eventloop(int fd, void *data, uint32_t code)
{

    if (code & (BEVENT_CODE_ERR | BEVENT_CODE_HUP)) {

	logoutput("read_signal_eventloop: error and/or hup");

    } else if (code & BEVENT_CODE_IN) {
	struct bevent_s *bevent=(struct bevent_s *) data;
	struct signalfd_siginfo fdsi;
	ssize_t len=0;

	len=read(fd, &fdsi, sizeof(struct signalfd_siginfo));

	if (len == sizeof(struct signalfd_siginfo)) {

	    logoutput("read_signal_eventloop: caught signal %i", fdsi.ssi_signo);

	    (* bevent->loop->cb_signal) (bevent->loop, bevent->data, &fdsi);
	    return (int)len;

	} else if (len==-1) {

    	    if (errno==EWOULDBLOCK||errno==EAGAIN ) return -1;

	}

    }

    return 0;

}

static int add_signalhandler(struct beventloop_s *loop, void (* cb) (struct beventloop_s *loop, void *data, struct signalfd_siginfo *fdsi), void *data, unsigned int *error)
{
    sigset_t sigset;
    struct bevent_s *bevent=NULL;
    int fd=-1;

    if (loop->flags & BEVENTLOOP_FLAG_SIGNAL) return -1;

    logoutput("add_signalhandler");

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) {

	*error=errno;
    	goto error;

    }

    fd = signalfd(-1, &sigset, 0);

    if (fd == -1) {

    	*error=errno;
    	goto error;

    }

    bevent=add_to_beventloop(fd, BEVENT_CODE_IN, read_signal_eventloop, data, NULL, loop);

    if (bevent==NULL) {

	*error=EIO;
	goto error;

    }

    bevent->data=(void *) bevent;
    loop->cb_signal= (cb) ? cb : default_signal_cb;
    set_bevent_name(bevent, "SIGNAL");
    bevent->flags|=BEVENT_FLAG_SIGNAL;
    loop->flags|=BEVENTLOOP_FLAG_SIGNAL;
    return 0;

    error:

    if (fd>0) close(fd);
    if (bevent) free(bevent);
    logoutput("add_signalhandler: error %i (%s)", *error, strerror(*error));
    return -1;

}

int enable_beventloop_signal(struct beventloop_s *loop, void (* cb) (struct beventloop_s *loop, void *data, struct signalfd_siginfo *fdsi), void *data, unsigned int *error)
{
    logoutput("enable_beventloop_signal");

    if (! loop) loop=get_mainloop();
    return add_signalhandler(loop, cb, data, error);
}

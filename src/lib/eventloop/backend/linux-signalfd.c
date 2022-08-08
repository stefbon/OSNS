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
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <syslog.h>

#include <sys/signalfd.h>

#include "libosns-misc.h"
#include "libosns-log.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

static const char *backend_name="bsignal_subsystem:signalfd";

struct bsignal_backend_s {
    struct bevent_s					*bevent;
    struct shared_signal_s				*signal;
    void						(* cb)(struct beventloop_s *loop, struct bsignal_event_s *bse);
};

struct bsignal_hlp_s {
    struct beventloop_s 				*loop;
    void						(* cb)(struct beventloop_s *loop, struct bsignal_event_s *bse);
    struct bsignal_event_s 				bse;
};

static void default_signal_cb(struct beventloop_s *loop, struct bsignal_event_s *bse)
{

    switch (bse->signo) {

	case SIGHUP:
	case SIGTERM:
	case SIGINT:
	case SIGSTOP:
	case SIGABRT:
	case SIGQUIT:

	    logoutput("default_signal_cb: caught (HUP/TERM/INT/STOP/ABRT/QUIT) signal %i", bse->signo);
	    stop_beventloop(loop);
	    break;

	case SIGUSR1:
	case SIGUSR2:

	    logoutput("default_signal_cb: caught USR1/2 signal %i", bse->signo);
	    break;

	case SIGIO:

	    logoutput("default_signal_cb: caught IO signal %u pid %u fd %u", bse->signo, bse->type.io.pid, bse->type.io.fd);
	    break;

	case SIGCHLD:

	    logoutput("default_signal_cb: caught CHLD signal %u pid %u fd %u", bse->signo, bse->type.io.pid, bse->type.io.fd);
	    break;

	default:

	    logoutput("default_signal_cb: caught unsupported signal %i", bse->signo);

    }

}

static void thread2handle_signal(void *ptr)
{
    struct bsignal_hlp_s *hlp=(struct bsignal_hlp_s *) ptr;

    logoutput_debug("thread2handle_signal");

    (* hlp->cb)(hlp->loop, &hlp->bse);
    free(hlp);
}

static struct bsignal_hlp_s *create_bsignal_hlp(struct bevent_s *bevent, struct signalfd_siginfo *fdsi)
{
    struct bsignal_hlp_s *hlp=malloc(sizeof(struct bsignal_hlp_s));

    if (hlp) {
	struct bevent_subsystem_s *subsys=(struct bevent_subsystem_s *) bevent->ptr;
	struct bsignal_backend_s *backend=(struct bsignal_backend_s *) subsys->buffer;

	memset(hlp, 0, sizeof(struct bsignal_hlp_s));

	logoutput_debug("create_bsignal_hlp: subsys defined %s", ((subsys) ? "y" : "n"));

	hlp->loop=get_eventloop_bevent(bevent);
	hlp->cb=backend->cb;
	hlp->bse.signo=fdsi->ssi_signo;

	logoutput_debug("create_bsignal_hlp: ready");

    }

    return hlp;

}

static void signal_bevent_cb(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    logoutput("signal_bevent_cb");

    if (signal_is_data(arg)) {
	int fd=(* bevent->ops->get_unix_fd)(bevent);
	struct signalfd_siginfo fdsi;
	ssize_t len=0;

	len=read(fd, &fdsi, sizeof(struct signalfd_siginfo));

	if (len == sizeof(struct signalfd_siginfo)) {
	    struct bsignal_hlp_s *hlp=NULL;

	    switch (fdsi.ssi_signo) {

		case SIGHUP:
		case SIGTERM:
		case SIGINT:
		case SIGSTOP:
		case SIGABRT:
		case SIGQUIT:
		case SIGUSR1:
		case SIGUSR2:

		    hlp=create_bsignal_hlp(bevent, &fdsi);

		    if (hlp) {

			hlp->bse.type.kill.pid=fdsi.ssi_pid;
			hlp->bse.type.kill.uid=fdsi.ssi_uid;

		    }

		    break;

		case SIGIO:

		    hlp=create_bsignal_hlp(bevent, &fdsi);

		    if (hlp) {

			hlp->bse.type.io.pid=fdsi.ssi_pid;
			hlp->bse.type.io.fd=fdsi.ssi_fd;
			hlp->bse.type.io.events=fdsi.ssi_band;

		    }

		    break;

		case SIGCHLD:

		    hlp=create_bsignal_hlp(bevent, &fdsi);

		    if (hlp) {

			hlp->bse.type.chld.pid=fdsi.ssi_pid;
			hlp->bse.type.chld.uid=fdsi.ssi_uid;
			hlp->bse.type.chld.status=fdsi.ssi_status;
			hlp->bse.type.chld.utime=fdsi.ssi_utime;
			hlp->bse.type.chld.stime=fdsi.ssi_stime;

		    }

		    break;

		default:

		    logoutput_debug("signal_bevent_cb: signo %i not supported", fdsi.ssi_signo);

	    } /* switch */

	    if (hlp) work_workerthread(NULL, -1, thread2handle_signal, (void *) hlp, NULL);

	}

    }

}

static int start_bsignal_backend(struct bevent_subsystem_s *subsys)
{
    struct bsignal_backend_s *backend=(struct bsignal_backend_s *) subsys->buffer;
    struct bevent_s *bevent=backend->bevent;
    int result=-1;

    logoutput("start_bsignal_subsystem (flags=%i)", subsys->flags);

    if (subsys->flags & BEVENT_SUBSYSTEM_FLAG_START) return 0;

    if (bevent) {
	int fd=-1;

#ifdef __linux__

	sigset_t sigset;

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

	/* exec a child/exit*/
	sigaddset(&sigset, SIGCHLD);

	/* custom signals */
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);

	if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) goto out;
	fd = signalfd(-1, &sigset, 0);

#else

	errno=ENOSYS;

#endif

	if (fd<0) {

	    logoutput_warning("start_bsignal_subsystem: unable to open signal fd - error %i (%s)", errno, strerror(errno));
	    goto out;

	}

	(* bevent->ops->set_unix_fd)(bevent, fd);
	add_bevent_watch(bevent);
	result=0;

	subsys->flags |= BEVENT_SUBSYSTEM_FLAG_START;

    };

    out:
    return result;

}

static void stop_bsignal_backend(struct bevent_subsystem_s *subsys)
{
    struct bsignal_backend_s *backend=(struct bsignal_backend_s *) subsys->buffer;

    if (subsys->flags & BEVENT_SUBSYSTEM_FLAG_STOP) return;

    if (backend->bevent) {
	struct bevent_s *bevent=backend->bevent;
	int fd=(* bevent->ops->get_unix_fd)(bevent);

	if (fd>=0) {

	    close(fd);
	    (* bevent->ops->set_unix_fd)(bevent, -1);

	}

	subsys->flags |= BEVENT_SUBSYSTEM_FLAG_STOP;

    }

}

static void clear_bsignal_backend(struct bevent_subsystem_s *subsys)
{
    struct bsignal_backend_s *backend=NULL;

    if (subsys->flags & BEVENT_SUBSYSTEM_FLAG_CLEAR) return;

    backend=(struct bsignal_backend_s *) subsys->buffer;

    if (backend->bevent) {
	struct bevent_s *bevent=backend->bevent;

	remove_list_element(&bevent->list);
	(* bevent->ops->free_bevent)(&bevent);
	backend->bevent=NULL;

    }

    subsys->flags |= BEVENT_SUBSYSTEM_FLAG_CLEAR;

}

static struct bevent_subsystem_ops_s signal_backend_ops = {
    .start_subsys				= start_bsignal_backend,
    .stop_subsys				= stop_bsignal_backend,
    .clear_subsys				= clear_bsignal_backend,
};

/* with linux create a signal subsys using signalfd
*/

int init_signalfd_subsystem(struct beventloop_s *loop, struct bevent_subsystem_s *subsys)
{
    unsigned int size=sizeof(struct bsignal_backend_s);
    struct bsignal_backend_s *backend=NULL;
    struct bevent_s *bevent=NULL;

    if (subsys==NULL || subsys->size < size) return size;
    backend=(struct bsignal_backend_s *) subsys->buffer;
    memset(backend, 0, size);

    backend->signal=loop->signal;
    backend->cb=default_signal_cb;
    subsys->name=backend_name;
    subsys->ops=&signal_backend_ops;

    bevent=create_fd_bevent(loop, (void *) subsys);
    if (bevent==NULL) {

	logoutput_warning("init_signalfd_subsystem: unable to alloc bevent");
	goto failed;

    }

    set_bevent_cb(bevent, (BEVENT_FLAG_CB_DATAAVAIL | BEVENT_FLAG_CB_ERROR | BEVENT_FLAG_CB_CLOSE), signal_bevent_cb);
    backend->bevent=bevent;
    return 0;

    failed:

    if (bevent) {

	free_bevent(&bevent);
	backend->bevent=NULL;

    }

    return -1;

}

void set_cb_signalfd_subsystem(struct bevent_subsystem_s *subsys, void (* cb)(struct beventloop_s *loop, struct bsignal_event_s *bse))
{
    struct bsignal_backend_s *backend=NULL;
    unsigned int size=sizeof(struct bsignal_backend_s);

    if (subsys==NULL || subsys->size < size) return;
    backend=(struct bsignal_backend_s *) subsys->buffer;
    backend->cb=cb;
}

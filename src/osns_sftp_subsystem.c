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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-error.h"

#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-users.h"

#include "osns_sftp_subsystem.h"
#include "ssh/subsystem/connection.h"

#include "ssh/subsystem/sftp/send.h"
#include "ssh/subsystem/sftp/receive.h"
#include "ssh/subsystem/sftp/init.h"
#include "ssh/subsystem/sftp/payload.h"

#define PIDFILE_PATH							"/run"

char *program_name=NULL;

static void workspace_signal_handler(struct beventloop_s *loop, struct bsignal_event_s *bse)
{

    logoutput("workspace_signal_handler: received %i", bse->signo);

    switch (bse->signo) {

	case SIGHUP:
	case SIGINT:
	case SIGTERM:
	case SIGABRT:
	case SIGSTOP:

	    logoutput("workspace_signal_handler: got signal (%i): terminating", bse->signo);
	    stop_beventloop(loop);
	    break;

	case SIGIO:

	    logoutput("workspace_signal_handler: SIGIO");

	    /*
	    TODO:
	    when receiving an SIGIO signal another application is trying to open a file
	    is this really the case?
	    then the fuse fs is the owner!?

	    note 	pid
			fd
	    */
	    break;

	case SIGPIPE:

	    logoutput("workspace_signal_handler: SIGPIPE");
	    break;

	case SIGCHLD:

	    logoutput("workspace_signal_handler: SIGCHLD");
	    break;

	case SIGUSR1:

	    logoutput("workspace_signal_handler: SIGUSR1");
	    /* TODO: use to reread the configuration ?*/
	    break;

	default:

    	    logoutput("workspace_signal_handler: received unknown %i signal", bse->signo);

    }

}

int main(int argc, char *argv[])
{
    int result=0;
    unsigned int error=0;
    struct bevent_s *bevent=NULL;
    struct sftp_subsystem_s sftp;
    char *pidfile=NULL;
    int id_signal_subsystem=0;

    switch_logging_backend("std");
    set_logging_level(LOG_DEBUG);
    logoutput("%s started", argv[0]);

    /* output to stdout/stderr is useless since daemonized */

    switch_logging_backend("syslog");
    set_logging_level(LOG_DEBUG);

    if (init_beventloop(NULL)==-1) {

        logoutput_error("MAIN: error creating eventloop.");
        goto post;

    } else {

	logoutput_info("MAIN: eventloop initialized");

    }

    id_signal_subsystem=create_bevent_signal_subsystem(NULL, workspace_signal_handler);

    if (id_signal_subsystem==-1) {

	logoutput_error("MAIN: error adding signal handler to eventloop.");
        goto out;

    } else {

	logoutput_info("MAIN: signal handler added to main eventloop");

    }

    result=start_bsignal_subsystem(NULL, id_signal_subsystem);
    logoutput("MAIN: signal handler started (%i)", result);

    if (init_hash_commonhandles(&error)==0) {

	logoutput_info("MAIN: initializing common handles (for open/read/write, opendir/readir, fstatat, mkdirat etc)");

    } else {

	logoutput_info("MAIN: error initializing common handles");
	goto out;

    }

    /* Initialize and start default threads
	NOTE: important to start these after initializing the signal handler, if not doing this this way any signal will make the program crash */

    init_workerthreads(NULL);
    set_max_numberthreads(NULL, 6); /* depends on the number of users and connected workspaces, 6 is a reasonable amount for this moment */
    start_default_workerthreads(NULL);

    if (init_sftp_subsystem(&sftp)==0) {

	logoutput("MAIN: sftp subsystem initialized");

    } else {

	logoutput_warning("MAIN: failed to initialize sftp subsystem");
	goto out;

    }

    if (connect_ssh_subsystem_connection(&sftp.connection)==0) {

	logoutput("MAIN: sftp subsystem connected");

    } else {

	logoutput_warning("MAIN: failed to connect sftp subsystem");
	goto out;

    }

    set_process_sftp_payload_init(&sftp);

    /* disable pidfile for now: find a location with write access first */

    // create_pid_file(PIDFILE_PATH, "osns_sftp_subsystem", sftp.identity.pwd.pw_name, getpid(), &pidfile);

    start_beventloop(NULL);

    out:

    clear_ssh_subsystem_connection(-1, &sftp.connection);

    logoutput_info("MAIN: stop workerthreads");
    stop_workerthreads(NULL);

    post:

    logoutput_info("MAIN: terminate workerthreads");
    terminate_workerthreads(NULL, 0);

    logoutput_info("MAIN: destroy eventloop");
    clear_beventloop(NULL);
    free_hash_commonhandles();

    if (pidfile) {

	remove_pid_file(pidfile);
	free(pidfile);

    }

    if (error>0) {

	logoutput_error("MAIN: error (error: %i).", error);
	return 1;

    }

    return 0;

}

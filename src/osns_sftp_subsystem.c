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
#include <sys/statvfs.h>
#include <sys/mount.h>

#include "log.h"

#include "main.h"
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "eventloop.h"
#include "users.h"

#include "misc.h"
#include "osns_sftp_subsystem.h"

#include "ssh/subsystem/sftp/send.h"
#include "ssh/subsystem/sftp/receive.h"
#include "ssh/subsystem/sftp/init.h"
#include "ssh/subsystem/sftp/connect.h"

#define PIDFILE_PATH							"/run"

char *program_name=NULL;

static void workspace_signal_handler(struct beventloop_s *bloop, unsigned int signo, pid_t pid, int fd)
{

    logoutput("workspace_signal_handler: received %i", signo);

    if ( signo==SIGHUP || signo==SIGINT || signo==SIGTERM ) {

	logoutput("workspace_signal_handler: got signal (%i): terminating", signo);
	stop_beventloop(loop);

	/*
	    TODO: send a signal to all available io contexes to stop waiting
	*/

    } else if ( signo==SIGIO ) {

	logoutput("workspace_signal_handler: SIGIO");

	/*
	    TODO:
	    when receiving an SIGIO signal another application is trying to open a file
	    is this really the case?
	    then the fuse fs is the owner!?

	    note 	fdsi->ssi_pid
			fdsi->ssi_fd
	*/

    } else if ( signo==SIGPIPE ) {

	logoutput("workspace_signal_handler: SIGPIPE");

    } else if ( signo==SIGCHLD ) {

	logoutput("workspace_signal_handler: SIGCHLD");

    } else if ( signo==SIGUSR1 ) {

	logoutput("workspace_signal_handler: SIGUSR1");

	/* TODO: use to reread the configuration ?*/

    } else {

        logoutput("workspace_signal_handler: received unknown %i signal", signo);

    }

}

int main(int argc, char *argv[])
{
    int res=0;
    unsigned int error=0;
    struct bevent_s *bevent=NULL;
    struct sftp_subsystem_s sftp;
    char *pidfile=NULL;

    switch_logging_backend("std");
    setlogmask(LOG_UPTO(LOG_DEBUG));

    logoutput("%s started", argv[0]);

    /* output to stdout/stderr is useless since daemonized */

    switch_logging_backend("syslog");

    if (init_beventloop(NULL)==-1) {

        logoutput_error("MAIN: error creating eventloop, error: %i (%s).", error, strerror(error));
        goto post;

    } else {

	logoutput_info("MAIN: initializing eventloop");

    }

    if (enable_beventloop_signal(NULL, workspace_signal_handler)==-1) {

	logoutput_error("MAIN: error adding signal handler to eventloop: %i (%s).", error, strerror(error));
        goto out;

    } else {

	logoutput_info("MAIN: adding signal handler to eventloop");

    }

    /* Initialize and start default threads
	NOTE: important to start these after initializing the signal handler, if not doing this this way any signal will make the program crash */

    init_workerthreads(NULL);
    set_max_numberthreads(NULL, 6); /* depends on the number of users and connected workspaces, 6 is a reasonable amount for this moment */
    start_default_workerthreads(NULL);

    /* add SDTIN fileno to the eventloop  */

    if (init_sftp_subsystem(&sftp)==0) {

	logoutput("MAIN: sftp subsystem initialized");

    } else {

	logoutput_warning("MAIN: failed to initialize sftp subsystem");
	goto out;

    }

    if (connect_sftp_connection(&sftp.connection)==0) {

	logoutput("MAIN: sftp subsystem connected");

    } else {

	logoutput_warning("MAIN: failed to connect sftp subsystem");
	goto out;

    }

    set_process_sftp_payload_init(&sftp);

    if (add_sftp_connection_eventloop(&sftp.connection)==0) {

	logoutput("MAIN: sftp subsystem added tot eventloop");

    } else {

	logoutput_warning("MAIN: failed to add sftp subsystem to eventloop");
	goto out;

    }

    /* disable pidfile for now: find a location with write access first */

    // create_pid_file(PIDFILE_PATH, "osns_sftp_subsystem", sftp.identity.pwd.pw_name, getpid(), &pidfile);

    res=start_beventloop(NULL);

    out:

    remove_sftp_connection_eventloop(&sftp.connection);
    disconnect_sftp_connection(&sftp.connection, 0);
    free_sftp_connection(&sftp.connection);

    logoutput_info("MAIN: stop workerthreads");
    stop_workerthreads(NULL);

    post:

    logoutput_info("MAIN: terminate workerthreads");
    terminate_workerthreads(NULL, 0);

    logoutput_info("MAIN: destroy eventloop");
    clear_beventloop(NULL);

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

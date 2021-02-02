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

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "log.h"

#include "main.h"
#include "misc.h"
#include "options.h"
#include "datatypes/ssh-string.h"

#include "threads.h"
#include "eventloop.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "fsnotify.h"
#include "users.h"
#include "mountinfo.h"

#include "misc.h"
#include "network.h"
#include "discover.h"

#include "fuse/network.h"

#include "interface/fuse.h"
#include "interface/sftp.h"
#include "interface/ssh.h"

struct fs_options_s fs_options;
char *program_name=NULL;

struct finish_script_s {
    void 			(* finish)(void *ptr);
    void			*ptr;
    char			*name;
    struct finish_script_s	*next;
};

static struct finish_script_s *finish_scripts=NULL;
static pthread_mutex_t finish_scripts_mutex=PTHREAD_MUTEX_INITIALIZER;

void add_finish_script(void (* finish_cb)(void *ptr), void *ptr, char *name)
{
    struct finish_script_s *script=NULL;

    script=malloc(sizeof(struct finish_script_s));

    if (script) {

	script->finish=finish_cb;
	script->ptr=ptr;
	script->name=name;
	script->next=NULL;

	pthread_mutex_lock(&finish_scripts_mutex);

	script->next=finish_scripts;
	finish_scripts=script;

	pthread_mutex_unlock(&finish_scripts_mutex);

    } else {

	logoutput_warning("add_finish_script: error allocating memory to add finish script");

    }

}

void run_finish_scripts()
{
    struct finish_script_s *script=NULL;

    pthread_mutex_lock(&finish_scripts_mutex);

    script=finish_scripts;

    while (script) {

	finish_scripts=script->next;

	if (script->name) logoutput_info("run_finish_scripts: run script %s", script->name);

	(* script->finish)(script->ptr);
	free(script);

	script=finish_scripts;

    }

    pthread_mutex_unlock(&finish_scripts_mutex);

}

void end_finish_scripts()
{
    pthread_mutex_destroy(&finish_scripts_mutex);
}

static void terminate_user_connections(struct osns_user_s *user)
{
    struct list_element_s *list=NULL;

    logoutput("terminate_user_connections");

    list=get_list_head(&user->connections, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct service_context_s *context=NULL;

	workspace=get_container_workspace(list);
	context=get_workspace_context(workspace);

	// _disconnect_workspace(&context->interface);

	logoutput("terminate_user_workspaces: free mount");
	free_workspace_mount(workspace);

	list=get_list_head(&user->workspaces, SIMPLE_LIST_FLAG_REMOVE);

    }

    logoutput("terminate_user_workspaces: ready");

}

static void end_osns_user_session(void *ptr)
{
    struct osns_user_s *user=(struct osns_user_s *) ptr;

    logoutput("end_osns_user_session: %i: %s", user->pwd.pw_uid, user->pwd.pw_name);
    terminate_user_workspaces(user);
    pthread_mutex_destroy(&user->mutex);
    free(user);
    user=NULL;

}

static void end_osns_user_sessions(void *ptr)
{
    struct osns_user_s *user=NULL;
    void *index=NULL;
    unsigned int hashvalue=0;
    struct simple_lock_s wlock;

    logoutput("end_osns_user_sessions");
    init_wlock_users_hash(&wlock);

    getuser:

    index=NULL;

    lock_users_hash(&wlock);
    user=get_next_osns_user(&index, &hashvalue);
    if (user) remove_osns_user_hash(user);
    unlock_users_hash(&wlock);

    if (user) {

	work_workerthread(NULL, 0, end_osns_user_session, (void *) user, NULL);
	index=NULL;
	goto getuser;

    }

}

static void add_osns_user_session(uid_t uid)
{
    struct osns_user_s *user=NULL;
    unsigned int error=0;

    logoutput_info("add_osns_user_session: %i", (int) uid);

    user=add_osns_user(uid, &error);

    if (user && error==0) {
	struct passwd *pwd=&user->pwd;
	char *mountpoint=NULL;

	if (fs_options.user.flags & _OPTIONS_USER_FLAG_NETWORK_GID_PARTOF) {

	    if (fs_options.user.network_mount_group>0) {
		struct group *grp=getgrgid(fs_options.user.network_mount_group);

		if (grp==NULL) return;

		if (pwd->pw_gid==fs_options.user.network_mount_group) {

		    logoutput("add_osns_user_session: user %i: %s is part of group %i: %s", pwd->pw_uid, pwd->pw_name, grp->gr_gid, grp->gr_name);

		} else if (user_is_groupmember(pwd->pw_name, grp)==1) {

		    logoutput("add_osns_user_session: user %i: %s is part of group %i: %s", pwd->pw_uid, pwd->pw_name, grp->gr_gid, grp->gr_name);

		} else {

		    logoutput("add_osns_user_session: user %i: %s is not part of group %i: %s", pwd->pw_uid, pwd->pw_name, grp->gr_gid, grp->gr_name);
		    return;

		}

	    }

	} else if (fs_options.user.flags & _OPTIONS_USER_FLAG_NETWORK_GID_MIN) {

	    if (pwd->pw_gid < _OPTIONS_USER_FLAG_NETWORK_GID_MIN) {

		logoutput("add_osns_user_session: user %i: %s group id %i is less than %i", pwd->pw_uid, pwd->pw_name, pwd->pw_gid, _OPTIONS_USER_FLAG_NETWORK_GID_MIN);
		return;

	    } else {

		logoutput("add_osns_user_session: user %i: %s group id %i is not less than %i", pwd->pw_uid, pwd->pw_name, pwd->pw_gid, _OPTIONS_USER_FLAG_NETWORK_GID_MIN);

	    }

	}

	mountpoint=get_path_from_template(fs_options.user.network_mount_template.path, pwd, NULL, 0);

	/* mountpoint */

	if (mountpoint) {

	    if (create_directory(mountpoint, S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, &error) == 0) {

		if (create_mount_context(user, &mountpoint, WORKSPACE_TYPE_NETWORK)) {

		    logoutput("add_osns_user_session: network mount context created");

		} else {

		    logoutput("add_osns_user_session: network mount context not created");

		}

	    } else {

		logoutput_error("add_osns_user_session: error %i:%s creating directory %s", error, strerror(error), mountpoint);

	    }

	    if (mountpoint) free(mountpoint);

	}

    }

}

static void change_usersessions(uid_t uid, signed char change, void *ptr)
{
    struct osns_user_s *user=NULL;
    struct simple_lock_s wlock;

    logoutput("change_usersession: %s user %i", (change==1) ? "add" : "remove", uid);

    init_wlock_users_hash(&wlock);
    lock_users_hash(&wlock);

    user=lookup_osns_user(uid);

    if (user) {

	if (change==-1) {

	    remove_osns_user_hash(user);
	    unlock_users_hash(&wlock);
	    work_workerthread(NULL, 0, end_osns_user_session, NULL, (void *) user);
	    return;

	}

    } else {

	if (change==1 || change==0) add_osns_user_session(uid);

    }

    unlock_users_hash(&wlock);

}

static void workspace_signal_handler(struct beventloop_s *bloop, void *data, struct signalfd_siginfo *fdsi)
{
    unsigned int signo=fdsi->ssi_signo;

    logoutput("workspace_signal_handler: received %i", signo);

    if ( signo==SIGHUP || signo==SIGINT || signo==SIGTERM ) {

	logoutput("workspace_signal_handler: got signal (%i): terminating", signo);
	bloop->status=BEVENTLOOP_STATUS_DOWN;

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
/* accept only connections from users with a complete session
    what api??
    SSH_MSG_CHANNEL_REQUEST...???
*/

struct fs_connection_s *accept_client_connection(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *s_conn)
{
    struct osns_user_s *user=NULL;
    struct simple_lock_s wlock;

    logoutput_info("accept_client_connection");
    init_wlock_users_hash(&wlock);

    lock_users_hash(&wlock);

    user=lookup_osns_user(uid);

    if (user) {
	struct fs_connection_s *c_conn=malloc(sizeof(struct fs_connection_s));

	if (c_conn) {

	    init_connection(c_conn, FS_CONNECTION_TYPE_LOCAL, FS_CONNECTION_ROLE_CLIENT);
	    unlock_users_hash(&wlock);
	    return c_conn;

	}

    }

    unlock:
    unlock_users_hash(&wlock);
    return NULL;
}

int main(int argc, char *argv[])
{
    int res=0;
    unsigned int error=0;
    struct bevent_s *bevent=NULL;
    struct fs_connection_s socket;

    switch_logging_backend("std");
    setlogmask(LOG_UPTO(LOG_DEBUG));

    logoutput("%s started", argv[0]);

    /* parse commandline options and initialize the fuse options */

    res=parse_arguments(argc, argv, &error);

    if (res==-1 || res==1) {

	if (res==-1 && error>0) {

	    if (error>0) logoutput_error("MAIN: error, cannot parse arguments, error: %i (%s).", error, strerror(error));

	}

	goto options;

    }

    /* daemonize */

    res=custom_fork();

    if (res<0) {

        logoutput_error("MAIN: error daemonize.");
        return 1;

    } else if (res>0) {

	logoutput("MAIN: created a service with pid %i.", res);
	return 0;

    }

    /* output to stdout/stderr is useless since daemonized */

    switch_logging_backend("syslog");

    if (initialize_osns_users(&error)==-1) {

	logoutput_error("MAIN: error, cannot initialize fuse users hash table, error: %i (%s).", error, strerror(error));
	goto post;

    }

    // init_backuphash();

    if (init_beventloop(NULL)==-1) {

        logoutput_error("MAIN: error creating eventloop, error: %i (%s).", error, strerror(error));
        goto post;

    } else {

	logoutput_info("MAIN: creating eventloop");

    }

    if (enable_beventloop_signal(NULL, workspace_signal_handler, NULL, &error)==-1) {

	logoutput_error("MAIN: error adding signal handler to eventloop: %i (%s).", error, strerror(error));
        goto out;

    } else {

	logoutput_info("MAIN: adding signal handler");

    }

    if (add_mountinfo_watch(NULL, &error)==-1) {

        logoutput_error("MAIN: unable to open mountmonitor, error=%i (%s)", error, strerror(error));
        goto out;

    } else {

	logoutput_info("MAIN: open mountmonitor");

    }

    add_mountinfo_source("network@osns.org");

    /* Initialize and start default threads
	NOTE: important to start these after initializing the signal handler, if not doing this this way any signal will make the program crash */

    init_workerthreads(NULL);
    set_max_numberthreads(NULL, 6); /* depends on the number of users and connected workspaces, 6 is a reasonable amount for this moment */
    start_default_workerthreads(NULL);

    if (init_fschangenotify(NULL, &error)==-1) {

	logoutput_error("MAIN: error initializing fschange notify, error: %i (%s)", error, strerror(error));
	goto out;

    }

    if (create_socket_path(&fs_options.socket)==0) {
	unsigned int alreadyrunning=0;
	unsigned int count=0;

	checkpidfile:

	alreadyrunning=check_pid_file(&fs_options.socket);

	if (alreadyrunning>0 && count < 10) {
	    char procpath[64];
	    struct stat st;

	    snprintf(procpath, 64, "/proc/%i/cmdline", alreadyrunning);

	    /* check here for existence of cmdline
		a better check will be to test also the cmdline contains this programname if it exists */

	    if (stat(procpath, &st)==-1) {

		/* pid file found, but no process, so it's not running: remove the pid file */

		remove_pid_file(&fs_options.socket, (pid_t) alreadyrunning);
		alreadyrunning=0;
		count++;
		goto checkpidfile;

	    } else {
		int fd=0;

		/* check the contents of the procfile cmdline: it should be the same as argv[0] */

		fd=open(procpath, O_RDONLY);

		if (fd>0) {
		    char buffer[PATH_MAX];
		    ssize_t bytesread=0;

		    memset(buffer, '\0', PATH_MAX);
		    bytesread=read(fd, buffer, PATH_MAX);
		    if (bytesread>0) {

			// if (strcmp(buffer, argv[0]) != 0) {

			    logoutput_info("MAIN: cmdline pid %i is %s", alreadyrunning, buffer);

			//}

		    }

		    close(fd);

		}

	    }

	}

	if (check_socket_path(&fs_options.socket, alreadyrunning)==-1) goto out;
	init_connection(&socket, FS_CONNECTION_TYPE_LOCAL, FS_CONNECTION_ROLE_SERVER);

	if (create_local_serversocket(fs_options.socket.path, &socket, NULL, accept_client_connection, NULL)>=0) {

	    logoutput_info("MAIN: created socket %s", fs_options.socket.path);

	} else {

	    logoutput_info("MAIN: error %i creating socket %s (%s)", error, fs_options.socket.path, strerror(error));
	    goto out;

	}

    } else {

	logoutput_info("MAIN: error creating directory for socket %s", fs_options.socket.path);

    }

    create_pid_file(&fs_options.socket);
    res=create_user_monitor(change_usersessions, NULL);

    if (res<0) {

	logoutput_error("MAIN: error initializing usersessions monitor, error: %i", res);
	goto out;

    } else {

	if (add_to_beventloop(res, BEVENT_CODE_IN, read_user_monitor_event, NULL, NULL, NULL)) {

	    logoutput("MAIN: added usermonitor %i to eventloop", res);

	} else {

	    logoutput("MAIN: failed to added usermonitor %i to eventloop", res);

	}

    }

    read_user_monitor_event(0, NULL, 0);

    res=start_beventloop(NULL);

    out:

    remove_mountinfo_watch();

    logoutput_info("MAIN: close sessions monitor");
    close_user_monitor();
    end_osns_user_sessions(NULL);

    logoutput_info("MAIN: end fschangenotify");
    end_fschangenotify();

    logoutput_info("MAIN: stop workerthreads");
    stop_workerthreads(NULL);

    post:

    logoutput_info("MAIN: terminate workerthreads");

    run_finish_scripts();
    end_finish_scripts();
    terminate_workerthreads(NULL, 0);

    logoutput_info("MAIN: destroy eventloop");
    clear_beventloop(NULL);

    free_osns_users();
    remove_pid_file(&fs_options.socket, getpid());

    options:

    logoutput_info("MAIN: free options");
    free_options();

    if (error>0) {

	logoutput_error("MAIN: error (error: %i).", error);
	return 1;

    }

    return 0;

}

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

#include "network.h"
#include "discover.h"

#include "fuse/network.h"

#include "interface/fuse.h"
#include "interface/sftp.h"
#include "interface/ssh.h"

#include "osns_socket.h"

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

static void _disconnect_workspace(struct context_interface_s *interface)
{
    struct service_context_s *root=get_service_context(interface);
    struct workspace_mount_s *workspace=NULL;
    unsigned int error=0;
    struct directory_s *root_directory=NULL;
    struct list_element_s *list=NULL;
    struct service_context_s *context=NULL;

    logoutput_info("_disconnect_workspace");

    workspace=root->workspace;
    if (! workspace) return;

    pthread_mutex_lock(&workspace->mutex);
    workspace->status=WORKSPACE_STATUS_UNMOUNTING;
    pthread_mutex_unlock(&workspace->mutex);

    (* interface->signal_interface)(interface, "command:disconnect:", NULL);
    (* interface->signal_interface)(interface, "command:close:", NULL);

    logoutput_info("_disconnect_workspace: umount");

    umount_fuse_interface(&workspace->mountpoint);

    logoutput_info("_disconnect_workspace: remove inodes, entries and directories");

    root_directory=remove_directory(&workspace->inodes.rootinode, &error);

    if (root_directory) {

	/* this will also close and free connections */
	clear_directory_recursive(interface, root_directory);
	free_directory(root_directory);

    }

    /* remove contexes in reverse order than added */

    list=get_list_tail(&workspace->contexes, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {

	context=get_container_context(list);
	logoutput_info("_disconnect_workspace: disconnect service %s context", context->name);

	(* context->interface.signal_interface)(&context->interface, "command:disconnect:", NULL);
	(* context->interface.signal_interface)(&context->interface, "command:close:", NULL);
	(* context->interface.signal_interface)(&context->interface, "command:free:", NULL);
	free_service_context(context);
	list=get_list_tail(&workspace->contexes, SIMPLE_LIST_FLAG_REMOVE);

    }

}

static int signal_ctx2fuse(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    logoutput_info("signal_ctx2fuse: what %s", what);
    return (* interface->signal_interface)(interface, what, option);
}

/* enable/disable flags for filesystem
    depends on the type:
    - flock enabled on filesystems
    - xattr ?
*/

static int get_mount_context_option(struct context_interface_s *interface, const char *name, struct ctx_option_s *option)
{
    struct service_context_s *context=get_service_context(interface);

    if (strcmp(name, "async-read")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "posix-locks")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "file-ops")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=1;

    } else if (strcmp(name, "atomic-o-trunc")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0; /* test this ... */

    } else if (strcmp(name, "export-support")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "big-writes")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=1;

    } else if (strcmp(name, "dont-mask")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "splice-write")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "splice-move")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "splice-read")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "flock-locks")==0) {

	option->type=_CTX_OPTION_TYPE_INT;

	// if (base->type==WORKSPACE_TYPE_NETWORK) {

	    option->value.integer=1;

	// } else {

	    // option->value.integer=0;

	// }

    } else if (strcmp(name, "has-ioctl-dir")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "auto-inval-data")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "do-readdirplus")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "readdirplus-auto")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "async-dio")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "writeback-cache")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=1;

    } else if (strcmp(name, "no-open-support")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0;

    } else if (strcmp(name, "parallel-dirops")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0; /* try */

    } else if (strcmp(name, "posix-acl")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=0; /* todo ... */

    }

    return sizeof(unsigned int);
}

static int signal_fuse2ctx(struct context_interface_s *interface, const char *name, struct ctx_option_s *option)
{
    if (strncmp(name, "option:", 7)==0) {

	return get_mount_context_option(interface, &name[7], option);

    }

    return -1;
}

struct service_context_s *create_mount_context(struct osns_user_s *user, char **p_mountpoint, unsigned char type)
{
    struct service_context_s *context=NULL;
    struct workspace_mount_s *workspace=NULL;
    unsigned int error=0;
    unsigned int count=build_interface_ops_list(NULL, NULL, 0);
    struct interface_list_s ailist[count];
    struct interface_list_s *ilist=NULL;

    /* build the list with available interface ops
	important here are of course the ops to setup a ssh server context and a sftp server context (=ssh channel) */

    for (unsigned int i=0; i<count; i++) {

	ailist[i].type=-1;
	ailist[i].name=NULL;
	ailist[i].ops=NULL;

    }

    count=build_interface_ops_list(NULL, ailist, 0);

    /* look for the interface ops for a ssh session */

    for (unsigned int i=0; i<(sizeof(ailist)/sizeof(ailist[0])); i++) {

	if (ailist[i].type==_INTERFACE_TYPE_FUSE) {

	    ilist=&ailist[i];
	    break;

	}

    }

    if (ilist==NULL) {

	error=EINVAL;
	logoutput_warning("create_mount_context: no handlers found to setup mountpoint");
	goto error;

    }

    logoutput("create_mount_context: for user %i:%s %s", (int) user->pwd.pw_uid, user->pwd.pw_name, user->pwd.pw_gecos);

    workspace=malloc(sizeof(struct workspace_mount_s));
    if (workspace==NULL) {

	logoutput("create_mount_context: unable to allocate memory for workspace");
	goto error;

    } else if (init_workspace_mount(workspace, &error)==-1) {

	logoutput_warning("create_mount_context: unable to initialize workspace");
	goto error;

    }

    workspace->user=user;
    workspace->type=type;
    workspace->mountpoint.path=*p_mountpoint;
    workspace->mountpoint.len=strlen(*p_mountpoint);
    workspace->mountpoint.flags=PATHINFO_FLAGS_ALLOCATED;
    workspace->mountpoint.refcount=1;
    *p_mountpoint=NULL;

    context=create_service_context(workspace, NULL, ilist, SERVICE_CTX_TYPE_WORKSPACE, NULL);

    if (context) {
	struct context_interface_s *interface=&context->interface;
	struct service_address_s service;
	struct inode_link_s link;
	int fd=-1;
	char source[64];
	char name[32];

	snprintf(source, 64, "fuse.network");
	snprintf(name, 32, "network");

	interface->signal_context=signal_fuse2ctx;
	register_fuse_functions(interface);
	use_virtual_fs(context, &workspace->inodes.rootinode);
	set_directory_dump(&workspace->inodes.rootinode, get_dummy_directory());

	/* connect to the fuse interface: mount */
	/* target address of interface is a local mountpoint */

	service.type=_SERVICE_TYPE_FUSE;
	service.target.fuse.source=source;
	service.target.fuse.mountpoint=workspace->mountpoint.path;
	service.target.fuse.name=name;

	logoutput("create_mount_context: connect");

	fd=(* context->interface.connect)(user->pwd.pw_uid, interface, NULL, &service);

	if (fd==-1) {

	    logoutput("create_mount_context: failed to mount %s", workspace->mountpoint.path);
	    goto error;

	}

	logoutput("create_mount_context: starting FUSE mountpoint using fd %i", fd);

	if ((* context->interface.start)(&context->interface, fd, NULL)==0) {
	    struct simple_lock_s wlock;
	    struct inode_s *inode=&workspace->inodes.rootinode;
	    struct directory_s *d=get_directory(inode);

	    (* user->add)(user, &workspace->list);
	    logoutput("create_mount_context: FUSE mountpoint %s mounted", workspace->mountpoint.path);

	    if (wlock_directory(d, &wlock)==0) {
		struct inode_link_s link;

		link.type=INODE_LINK_TYPE_CONTEXT;
		link.link.ptr=(void *) context;
		set_inode_link_directory(inode, &link);
		unlock_directory(d, &wlock);

	    }

	    return context;

	}

	logoutput("create_mount_context: failed to start %s", workspace->mountpoint.path);
	close(fd);

    } else {

	logoutput("create_mount_context: failed to create mount context");

    }

    error:

    if (context) free_service_context(context);
    if (workspace) free_workspace_mount(workspace);
    return NULL;

}

static void terminate_user_workspaces(struct osns_user_s *user)
{
    struct workspace_mount_s *workspace=NULL;
    struct list_element_s *list=NULL;

    logoutput("terminate_user_workspaces");

    list=get_list_head(&user->header, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct service_context_s *context=NULL;

	workspace=get_container_workspace(list);
	context=get_workspace_context(workspace);

	_disconnect_workspace(&context->interface);

	logoutput("terminate_user_workspaces: free mount");
	free_workspace_mount(workspace);

	list=get_list_head(&user->header, SIMPLE_LIST_FLAG_REMOVE);

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

	logoutput("add_osns_user_session: %i:%s", (pwd) ? pwd->pw_uid : (uid_t) -1, (pwd) ? pwd->pw_name : "null");

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

    init_fusesocket_interface();
    init_sftp_client_interface();
    init_ssh_session_interface();

    init_hashtable_fusesocket();
    init_directory_calls();
    init_special_fs();

    if (init_discover_group(&error)==-1) {

	logoutput_error("MAIN: error, cannot initialize discover group, error: %i (%s).", error, strerror(error));
	goto post;

    } else {

	logoutput_info("MAIN: initialized discover group");

    }

    set_discover_net_cb(install_net_services_cb);

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

	if (create_local_serversocket(fs_options.socket.path, &socket, NULL, accept_client_connection_from_localsocket, NULL)>=0) {

	    logoutput_info("MAIN: created socket %s", fs_options.socket.path);

	} else {

	    logoutput_info("MAIN: error %i creating socket %s (%s)", error, fs_options.socket.path, strerror(error));
	    goto out;

	}

    } else {

	logoutput_info("MAIN: error creating directory for socket %s", fs_options.socket.path);

    }

    create_pid_file(&fs_options.socket);

    if (fs_options.network.flags & _OPTIONS_NETWORK_DISCOVER_METHOD_FILE) {

	browse_services_staticfile(&fs_options.network.discover_static_file);

    }

    browse_services_avahi();

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

    free_special_fs();
    // stop_workerthreads(NULL);

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

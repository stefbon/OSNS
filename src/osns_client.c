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
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse-public.h"
#include "libosns-network.h"
#include "libosns-db.h"
#include "libosns-resource.h"
#include "libosns-ssh.h"

#include "osns-protocol.h"
#include "osns/osns.h"
#include "osns_client.h"

#include "osns/send/init.h"
#include "osns/utils.h"
#include "osns/connection.h"
#include "osns/send/write.h"
#include "osns/pidfile.h"

#include "osns/send/hashtable.h"
#include "osns/send/channel.h"
#include "osns/send/reply.h"

#include "osns/recv/msg-channel.h"
#include "osns/recv/msg-disconnect.h"
#include "osns/recv/msg-init.h"
#include "osns/recv/read.h"

#include "interface/fuse.h"

#include "client/workspaces.h"
#include "client/contexes.h"
#include "client/arguments.h"
#include "client/config.h"
#include "client/network.h"
#include "client/channel/channel.h"
#include "client/watch.h"

#include "interface/fuse.h"
#include "interface/sftp.h"
#include "interface/osns-channel.h"
#include "interface/ssh-channel.h"
#include "interface/ssh.h"

static char osns_static_buffer[OSNS_CONNECTION_BUFFER_SIZE];
static char osns_static_cbuffer[OSNS_CMSG_BUFFER_SIZE];
static int id_signal_subsystem=0;
static int id_timer_subsystem=0;
static char *pidfile=NULL;

/*static void process_netcache_change(char *data, unsigned int size, void *ptr)
{*/
    /* TODO:
	format of data is:
	uint32						valid
	NETCACHE ATTR					attr
	 */

/*    struct query_netcache_attr_s attr;
    int len=0;
    unsigned int pos=0;
    unsigned int valid=0;

    memset(&attr, 0, sizeof(struct query_netcache_attr_s));
    valid=get_uint32(&data[pos]);
    pos+=4;

    len=read_record_netcache(&data[pos], (size-pos), valid, &attr);

    if (len>0) {

	if (len + pos < size) logoutput_warning("process_netcache_change: less bytes (read: %u) than available (%u)", (len), (size - pos));
	process_netcache_attr(&attr, OSNS_NETCACHE_QUERY_FLAG_EVENT);

    }

}*/

static void start_mounting_workspaces(void *ptr)
{
    struct client_session_s *session=(struct client_session_s *) ptr;
    struct system_timespec_s expire;
    struct beventloop_s *loop=get_default_mainloop();
    struct service_context_s *ctx=NULL;
    struct connection_s *c=&session->osns.connection;
    struct osns_connection_s *oc=&session->osns;
    unsigned int sr=0;
    unsigned int sp=0;
    int result=-1;

    /* since this thread is started before the eventloop is up and running, wait here
	for the eventloop to be effective */

    get_current_time_system_time(&expire);
    system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 2);

    if (signal_wait_flag_set(loop->signal, &loop->flags, BEVENTLOOP_FLAG_START, &expire)==-1) {

	logoutput_warning("start_mounting_workspaces: waiting for main eventloop to start, but failed ... cannot continue");
	goto finishout;

    }

    /* do init of the osns protocol with server ... required is the mount capability */

    logoutput_debug("start_mounting_workspaces: eventloop started ... start osns initialization");

    /* create connection to the osns system service */

    if (create_osns_local_connection(c, OSNS_DEFAULT_RUNPATH, "systemsocket", loop, NULL)==0) {

	logoutput_info("start_mounting_workspaces: created connection to system socket");

    } else {

	logoutput_info("start_mounting_workspaces: error creating connection");
	goto finishout;

    }

    /* requested from osns system:
	- mount a fuse network filesystem
    */

    sr=(OSNS_INIT_FLAG_MOUNT_NETWORK);

    // | OSNS_INIT_FLAG_WATCH_NETCACHE);

    /* for now client provides nothing
	20221017: todo
	- open a channel to: execute a file, open a shell or start a ssh subsystem
	- list connections1
    */

    sp=0;

    do_osns_initialization(&session->osns, sr, sp);

    if (oc->status & OSNS_CONNECTION_STATUS_VERSION) {

	if (get_osns_major(oc->protocol.version)==1) {

	    if (oc->protocol.level.one.sr >= sr) result=0;

	 }

    }

    if (result==-1) {

	/* peer supports less than requested */

	logoutput_warning("start_mounting_workspaces: not all required services %u supported by server (received from server %u)", sr, oc->protocol.level.one.sr);
	goto finishout;

    }

    /* mount the network: request the osns system to do that  */

    logoutput_debug("start_mounting_workspaces: create mount context");
    ctx=create_mount_context(session, OSNS_MOUNT_TYPE_NETWORK, session->options.fuse.maxread);

    if (ctx) {

	logoutput_debug("start_mounting_workspaces: workspace mount context created");

	/* query the osns system for network resources
	    these resources are required to provide the user a browseable map through the "network environment" */

	populate_network_workspace_mount(ctx);

    } else {

	logoutput_debug("start_mounting_workspaces: unable to create workspace mount context");

    }

    finishout:
    if (ctx==NULL) stop_beventloop(NULL, 0);

}

static void close_osns_connection(struct osns_connection_s *oc, unsigned int flags)
{
}

static void clear_osns_connection(struct osns_connection_s *oc)
{
}

static void workspace_signal_handler(struct beventloop_s *loop, struct bsignal_event_s *bse)
{

    switch (bse->signo) {

	case SIGHUP:
	case SIGTERM:
	case SIGINT:
	case SIGSTOP:
	case SIGABRT:
	case SIGQUIT:

	    logoutput("workspace_signal_handler: got signal (%i): terminating", bse->signo);

	    // if (id_signal_subsystem>0) {

		// stop_bsignal_subsystem(NULL, id_signal_subsystem); /* prevent the "catching" of the eventual following signal */
		// id_signal_subsystem=0;

	    // }

	    /* stop eventloop will finish the program, and send the signal again (for attached subsystems which also listen to signals)
		but did not receive anything cause the signal handler was in the way */

	    stop_beventloop(loop, bse->signo);
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

static struct osns_ops_s client_ops = {
    .init				= NULL,
    .openquery				= NULL,

    /* client cannot do a mount and will never receive a request to do that */
    .mount				= NULL,
    .umount				= NULL,

    .openchannel			= process_osns_channel_open_client,
    .startchannel			= process_osns_channel_start_client,

    .watchevent                         = NULL,
};

int main(int argc, char *argv[])
{
    int result=0;
    unsigned int error=0;
    struct client_arguments_s arguments;
    struct client_session_s session;
    struct osns_connection_s *oc=NULL;
    struct connection_s *c=NULL;
    struct osns_socket_s *sock=NULL;

    /* log to the standard output/error */

    switch_logging_backend("std");
    set_logging_level(LOG_DEBUG);
    logoutput("%s started", argv[0]);

    /* parse commandline arguments  */

    memset(&arguments, 0, sizeof(struct client_arguments_s));
    result=parse_arguments(argc, argv, &arguments);

    if (result==-1 || result==1) {

	return ((result==1) ? 0 : -1);

    }

    logoutput_debug("MAIN: commandline arguments parsed ...");

    /* daemonize */

    result=custom_fork();

    if (result<0) {

        logoutput_error("MAIN: error daemonize.");
        return -1;

    } else if (result>0) {

	logoutput("MAIN: created a service with pid %i.", result);
	return 0;

    }

    /* output to stdout/stderr is useless since daemonized so use syslog */

    switch_logging_backend("syslog");
    set_logging_level(LOG_DEBUG); /* make this configurable */

    if (check_create_pid_file(OSNS_DEFAULT_RUNPATH, argv[0], &pidfile)==-1) {

	logoutput_error("MAIN: cannot continue, %s already running", argv[0]);
	goto out;

    }

    /* main client session for io with osns system */

    memset(&session, 0, sizeof(struct client_session_s));
    oc=&session.osns;
    c=&oc->connection;

    /* session */

    session.status=0;
    session.signal=get_default_shared_signal();
    init_list_header(&session.workspaces, SIMPLE_LIST_TYPE_EMPTY, NULL);

    oc->flags=OSNS_CONNECTION_FLAG_CLIENT;
    oc->status=0;
    oc->close=close_osns_connection;
    oc->clear=clear_osns_connection;
    oc->free=clear_osns_connection;
    oc->protocol.version=create_osns_version(OSNS_CLIENT_VERSION, OSNS_CLIENT_MINOR);
    oc->ops=&client_ops;

    set_default_options(&session.options);

    if (read_configfile(&session.options, &arguments)==-1) {

	logoutput("MAIN: failed to read option file/error");
	goto out;

    }

    init_db_sql();

    if (open_network_db(session.options.runpath)==0) {

        logoutput_debug("MAIN: open db with network resources");

    } else {

        logoutput_debug("MAIN: unable to open db with network resources ... cannot continue");
        goto out;

    }

    logoutput_debug("MAIN: initializing interfaces");

    init_context_interfaces(); 			/* init the list with all available interfaces */
    init_fuse_interface();			/* init/add the fuse interface */
    init_ssh_session_interface();		/* init/add ssh session interface */
    init_ssh_channel_interface();		/* init/add ssh channel interface */
    init_sftp_client_interface();		/* init/add sftp client interface */
    init_osns_channel_interface();		/* init/add OSNS channel */

    init_inode_hashtable();
    init_rootentry();
    init_dummy_directory();

    init_fuse_open_hashtable();			/* init the list for fuse handles */
    init_osns_send_hashtable();			/* init the client hashtable for io with osns system */

    /* init eventloop */

    if (init_beventloop(NULL)==-1) {

        logoutput_error("MAIN: error creating eventloop");
        goto post;

    } else {

	logoutput_info("MAIN: main eventloop created");

    }

    /* start subsystem eventloop system signals */

    id_signal_subsystem=create_bevent_signal_subsystem(NULL, workspace_signal_handler);

    if (id_signal_subsystem==-1) {

	logoutput_error("MAIN: error adding signal handler to eventloop.");
        goto out;

    }

    result=start_bsignal_subsystem(NULL, id_signal_subsystem);
    logoutput("MAIN: signal handler started (id=%u)", id_signal_subsystem);

    /* start subsystem eventloop system timer */

    id_timer_subsystem=create_bevent_timer_subsystem(NULL);

    if (id_timer_subsystem==-1) {

	logoutput_error("MAIN: error adding timer handler to eventloop.");
        goto out;

    }

    result=start_btimer_subsystem(NULL, id_timer_subsystem);
    logoutput("MAIN: timer handler started (id=%u)", id_timer_subsystem);

    /* Initialize and start default threads
	NOTE: important to start these after initializing the signal handler,
	if not doing this this way any signal will make the program crash */

    logoutput("MAIN: init workerthreads (max=%u)", session.options.maxthreads);

    init_workerthreads(NULL);
    set_max_numberthreads(NULL, session.options.maxthreads);
    start_default_workerthreads(NULL);

    init_connection(c, CONNECTION_TYPE_LOCAL, CONNECTION_ROLE_CLIENT, 0);
    sock=&c->sock;

    oc->signal=session.signal;
    oc->send=osns_connection_send_data;
    for (unsigned int i=0; i<= OSNS_MSG_MAX; i++) oc->cb[i]=reply_msg_notsupported;

    oc->cb[OSNS_MSG_INIT]=reply_msg_protocolerror;
    oc->cb[OSNS_MSG_VERSION]=process_msg_reply;
    oc->cb[OSNS_MSG_DISCONNECT]=process_msg_disconnect;
    oc->cb[OSNS_MSG_UNIMPLEMENTED]=process_msg_reply;

    /* open a channel, done by ctl or app forwarded by system */

    oc->cb[OSNS_MSG_CHANNEL_OPEN]=process_msg_channel_open;
    oc->cb[OSNS_MSG_CHANNEL_START]=process_msg_channel_start;

    /* allowed replies */

    oc->cb[OSNS_MSG_STATUS]=process_msg_reply;
    oc->cb[OSNS_MSG_NAME]=process_msg_reply;
    oc->cb[OSNS_MSG_MOUNTED]=process_msg_reply;
    oc->cb[OSNS_MSG_RECORDS]=process_msg_reply;

    /* socket */

    set_osns_socket_ops(sock, osns_static_buffer, OSNS_CONNECTION_BUFFER_SIZE);
    set_osns_socket_control_data_buffer(sock, osns_static_cbuffer, OSNS_CMSG_BUFFER_SIZE);

    /* start a thread to do all the io with osns system
	(to mount the workspaces)
	and let the main handle do the eventloop */

    logoutput_debug("MAIN: connect system using osns version %u", session.osns.protocol.version);

    work_workerthread(NULL, 0, start_mounting_workspaces, (void *) &session);
    start_beventloop(NULL);

    out:

    logoutput_debug("MAIN: remove workspaces");
    remove_workspaces(&session);

    logoutput_debug("MAIN: stop workerthreads");
    stop_workerthreads(NULL);

    post:

    terminate_workerthreads(NULL, 1);
    logoutput_info("MAIN: clear eventloop");
    clear_beventloop(NULL);

    if (pidfile) {

	remove_pid_file(pidfile);
	free(pidfile);

    }

    options:

    free_arguments(&arguments);
    free_options(&session.options);
    clear_shared_signal(&session.signal);
    return 0;

}

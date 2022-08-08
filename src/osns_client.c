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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse-public.h"
#include "libosns-network.h"
#include "libosns-resources.h"

#include "osns-protocol.h"
#include "osns_client.h"
#include "osns/init.h"
#include "osns/utils.h"
#include "osns/write.h"
#include "osns/client.h"
#include "osns/pidfile.h"
#include "osns/hashtable.h"
#include "osns/netcache-send.h"
#include "osns/netcache-read.h"

#include "interface/fuse.h"

#include "client/workspaces.h"
#include "client/contexes.h"
#include "client/arguments.h"
#include "client/config.h"
#include "client/network.h"

static char osns_static_buffer[OSNS_CONNECTION_BUFFER_SIZE];

/* process the received netcache data */

static void process_netcache_attr(struct query_netcache_attr_s *attr, unsigned int flags)
{
    unsigned int valid=attr->valid;
    char domainname[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
    struct host_address_s address;
    struct network_port_s port;
    unsigned int service=0;
    unsigned int transport=0;

    if (attr->flags & OSNS_NETCACHE_QUERY_FLAG_LOCALHOST) flags |= NETWORK_RESOURCE_FLAG_LOCALHOST;
    if (attr->flags & OSNS_NETCACHE_QUERY_FLAG_DNSSD) flags |= NETWORK_RESOURCE_FLAG_DNSSD;
    if (attr->flags & OSNS_NETCACHE_QUERY_FLAG_PRIVATE) flags |= NETWORK_RESOURCE_FLAG_PRIVATE;

    memset(&address, 0, sizeof(struct host_address_s));
    memset(&port, 0, sizeof(struct network_port_s));
    memset(domainname, 0, HOST_HOSTNAME_FQDN_MAX_LENGTH + 1);

    if ((valid & (OSNS_NETCACHE_QUERY_ATTR_IPV4 | OSNS_NETCACHE_QUERY_ATTR_IPV6))==0) {

	logoutput_debug("process_netcache_attr: no ip address from system attr");
	return;

    }

    if ((valid & OSNS_NETCACHE_QUERY_ATTR_PORT)==0) {

	logoutput_debug("process_netcache_attr: no port from system attr");
	return;

    }

    if ((valid & OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY)==0) {

	logoutput_debug("process_netcache_attr: no domain/communication family (IPv6, IPv4, ...) from system attr");
	return;

    }

    if ((valid & OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE)==0) {

	logoutput_debug("process_netcache_attr: no communication type (stream or dgram) from system attr");
	return;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV4) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_IPV4];

	if (name->len <= INET_ADDRSTRLEN) {

	    memcpy(address.ip.addr.v4, name->ptr, name->len);
	    address.flags |= HOST_ADDRESS_FLAG_IP;
	    address.ip.family=IP_ADDRESS_FAMILY_IPv4;

	    if (strncmp(address.ip.addr.v4, "127.", 4)==0) flags |= NETWORK_RESOURCE_FLAG_LOCALHOST;

	} else {

	    logoutput_warning("process_netcache_attr: ip4 number from system too long (%u, max %u)", name->len, INET_ADDRSTRLEN);

	}

    }

    if ((address.flags & HOST_ADDRESS_FLAG_IP)==0 && (valid & OSNS_NETCACHE_QUERY_ATTR_IPV6)) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_IPV6];

	if (name->len <= INET6_ADDRSTRLEN) {

	    memcpy(address.ip.addr.v6, name->ptr, name->len);
	    address.flags |= HOST_ADDRESS_FLAG_IP;
	    address.ip.family=IP_ADDRESS_FAMILY_IPv6;

	    if (strncmp(address.ip.addr.v6, "fe80:", 5)==0) flags |= NETWORK_RESOURCE_FLAG_LOCALHOST;

	} else {

	    logoutput_warning("process_netcache_attr: ip6 number from system too long (%u, max %u)", name->len, INET_ADDRSTRLEN);

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSHOSTNAME];

	if (name->len <= HOST_HOSTNAME_FQDN_MAX_LENGTH) {

	    memcpy(address.hostname, name->ptr, name->len);
	    address.flags |= HOST_ADDRESS_FLAG_HOSTNAME;

	    if (strcmp(address.hostname, "localhost")==0) flags |= NETWORK_RESOURCE_FLAG_LOCALHOST;

	} else {

	    logoutput_warning("process_netcache_attr: hostname from system too long (%u, max %u)", name->len, HOST_HOSTNAME_FQDN_MAX_LENGTH);

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN];

	if (name->len <= HOST_HOSTNAME_FQDN_MAX_LENGTH) {

	    memcpy(domainname, name->ptr, name->len);

	} else {

	    logoutput_warning("process_netcache_attr: domainname from system too long (%u, max %u)", name->len, HOST_HOSTNAME_FQDN_MAX_LENGTH);

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_PORT) {

	port.nr=attr->port;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE) {

	port.type=attr->comm_type;

    }

    /* service type, like SSH, SFTP, NFS, ... */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_SERVICE) {

	service=attr->service;

    }

    if (service==0) {

	service=guess_network_service_from_port(attr->port);
	if (service>0) flags |= NETWORK_RESOURCE_FLAG_SERVICE_GUESSED;

    }

    /* (secure) transport used ? */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_TRANSPORT) {

	transport=attr->transport;

    }

    add_network_service_resource(&address, domainname, &port, service, transport, flags);

}

static void process_netcache_change(char *data, unsigned int size, void *ptr)
{
    /* TODO:
	format of data is:
	uint32						valid
	NETCACHE ATTR					attr
	 */

    struct query_netcache_attr_s attr;
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

}

static void start_mounting_workspaces(void *ptr)
{
    struct client_session_s *session=(struct client_session_s *) ptr;
    struct system_timespec_s expire;
    struct beventloop_s *loop=get_default_mainloop();
    struct service_context_s *ctx=NULL;
    struct connection_s *c=&session->osns.connection;
    unsigned int requested_flags=0;
    unsigned int received_flags=0;

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

    if (create_local_connection(c, OSNS_DEFAULT_RUNPATH, loop)==0) {

	logoutput_info("start_mounting_workspaces: created connection to system socket");

    } else {

	logoutput_info("start_mounting_workspaces: error creating connection");
	goto finishout;

    }

    /* when contacting the osns system service at least mounting and quering the netcache should be supported
	and - cause dns names are neccesary - dns translation of the ip addresses is required

	TODO: add a watch for netcache
    */

    requested_flags=(OSNS_INIT_FLAG_MOUNT | OSNS_INIT_FLAG_NETCACHE | OSNS_INIT_FLAG_DNSLOOKUP | OSNS_INIT_FLAG_SETWATCH_NETCACHE);
    process_osns_initialization(&session->osns, requested_flags);
    received_flags=get_osns_protocol_flags(&session->osns);

    if ((received_flags & requested_flags) < requested_flags) {

	logoutput_warning("start_mounting_workspaces: not all required services %u supported by server", requested_flags);
	goto finishout;

    }

    /* mount the network */

    logoutput_debug("start_mounting_workspaces: create mount context");

    ctx=create_mount_context(session, OSNS_MOUNT_TYPE_NETWORK, session->options.fuse.maxread);

    if (ctx) {

	logoutput_debug("start_mounting_workspaces: workspace mount context created");

	/* query the osns system for network resources
	    these resources are required to provide the user a browseable map through the "network environment" */

	osns_system_query_netcache(&session->osns, NULL, process_netcache_attr);
	populate_network_workspace_mount(ctx);

    } else {

	logoutput_debug("start_mounting_workspaces: unable to create workspace mount context");

    }

    finishout:
    if (ctx==NULL) stop_beventloop(NULL);

}

static void workspace_signal_handler(struct beventloop_s *loop, struct bsignal_event_s *bse)
{

    logoutput("workspace_signal_handler: received %i", bse->signo);

    switch (bse->signo) {

	case SIGHUP:
	case SIGTERM:
	case SIGINT:
	case SIGSTOP:
	case SIGABRT:
	case SIGQUIT:

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
    char *pidfile=NULL;
    int id_signal_subsystem=0;
    int id_timer_subsystem=0;
    struct client_arguments_s arguments;
    struct client_session_s session;
    struct connection_s *c=NULL;
    struct osns_receive_s *r=NULL;

    switch_logging_backend("std");
    set_logging_level(LOG_DEBUG);
    logoutput("%s started", argv[0]);

    /* parse commandline options and initialize the fuse options */

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

    /* output to stdout/stderr is useless since daemonized */

    switch_logging_backend("syslog");
    set_logging_level(LOG_DEBUG);

    if (check_create_pid_file(OSNS_DEFAULT_RUNPATH, argv[0], &pidfile)==-1) {

	logoutput_error("MAIN: cannot continue, %s already running", argv[0]);
	goto out;

    }

    init_localhost_resources(NULL);
    result=add_network_subsys(NULL, NULL);

    if (result==1) {

	logoutput_debug("MAIN: added network resources subsys");

    } else if (result==-1) {

	logoutput_debug("MAIN: error adding network resources subsys");
	goto out;

    }

    logoutput_debug("MAIN: initializing interfaces");

    init_context_interfaces();
    init_fuse_interface();
    init_ssh_session_interface();
    init_ssh_channel_interface();
    init_sftp_client_interface();

    /* eventloop */

    if (init_beventloop(NULL)==-1) {

        logoutput_error("MAIN: error creating eventloop");
        goto post;

    } else {

	logoutput_info("MAIN: main eventloop created");

    }

    /* subsystem eventloop system signals */

    id_signal_subsystem=create_bevent_signal_subsystem(NULL, workspace_signal_handler);

    if (id_signal_subsystem==-1) {

	logoutput_error("MAIN: error adding signal handler to eventloop.");
        goto out;

    } else {

	logoutput_info("MAIN: signal handler added to main eventloop");

    }

    result=start_bsignal_subsystem(NULL, id_signal_subsystem);
    logoutput("MAIN: signal handler started (%i)", result);

    /* subsystem eventloop system timer */

    id_timer_subsystem=create_bevent_timer_subsystem(NULL);

    if (id_timer_subsystem==-1) {

	logoutput_error("MAIN: error adding timer handler to eventloop.");
        goto out;

    }

    logoutput("MAIN: timer handler added to main eventloop (id=%i)", id_timer_subsystem);

    result=start_btimer_subsystem(NULL, id_timer_subsystem);
    logoutput("MAIN: timer handler started (%i)", result);

    /* Initialize and start default threads
	NOTE: important to start these after initializing the signal handler,
	if not doing this this way any signal will make the program crash */

    /* TODO:
	- add maximum number of threads to options/configfile (6=reasonable at this moment)
    */

    init_workerthreads(NULL);
    set_max_numberthreads(NULL, 6);
    start_default_workerthreads(NULL);

    init_osns_client_hashtable();

    /* main client session */

    memset(&session, 0, sizeof(struct client_session_s));
    c=&session.osns.connection;
    r=&session.osns.receive;

    session.status=0;
    session.signal=get_default_shared_signal();

    session.osns.status=0;
    session.osns.protocol.version=create_osns_version(OSNS_CLIENT_VERSION, OSNS_CLIENT_MINOR);

    set_default_options(&session.options);

    if (read_configfile(&session.options, &arguments)==-1) {

	logoutput("MAIN: failed to read option file/error");
	goto out;

    }

    init_connection(c, CONNECTION_TYPE_LOCAL, CONNECTION_ROLE_CLIENT, 0);
    c->ops.client.error=osns_client_handle_error;
    c->ops.client.dataavail=osns_client_handle_dataavail;

    /* receive */

    r->status=0;
    r->ptr=(void *) c;
    r->signal=get_default_shared_signal();

    r->process_data=osns_client_process_data;
    r->send=osns_client_send_data;
    r->read=0;
    r->size=OSNS_CONNECTION_BUFFER_SIZE;
    r->threads=0;
    r->buffer=osns_static_buffer;

    init_list_header(&session.workspaces, SIMPLE_LIST_TYPE_EMPTY, NULL);

    /* start a thread to do all the io with osns system,
	and let the main handle the eventloop */

    work_workerthread(NULL, 0, start_mounting_workspaces, (void *) &session, NULL);
    start_beventloop(NULL);

    out:

    logoutput_info("MAIN: stop workerthreads");
    stop_workerthreads(NULL);

    post:

    terminate_workerthreads(NULL, 0);
    logoutput_info("MAIN: clear eventloop");
    clear_beventloop(NULL);

    if (pidfile) {

	remove_pid_file(pidfile);
	free(pidfile);

    }

    options:

    logoutput_info("MAIN: free options");
    free_arguments(&arguments);
    return 0;

}

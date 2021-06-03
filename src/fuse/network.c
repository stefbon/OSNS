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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <dirent.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "misc.h"
#include "threads.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "discover.h"
#include "discover/discover.h"

#include "fuse/sftp.h"
#include "fuse/ssh.h"
#include "network.h"
#include "workspace-fs.h"

extern struct fs_options_s fs_options;

static int test_buffer_ip(char *hostname)
{
    if (check_family_ip_address(hostname, "ipv4")==1) return 0;
    if (check_family_ip_address(hostname, "ipv6")==1) return 0;
    return -1;
}

static struct interface_list_s *find_interface_list(struct interface_list_s *ailist, unsigned int count, unsigned int type)
{

    /* build the list with available interface ops */

    for (unsigned int i=0; i<count; i++) {

	ailist[i].type=-1;
	ailist[i].name=NULL;
	ailist[i].ops=NULL;

    }

    count=build_interface_ops_list(NULL, ailist, 0);
    return (type>0) ? get_interface_list(ailist, count, type) : NULL;

}

struct service_context_s *get_next_network_browse_context(struct workspace_mount_s *workspace, struct service_context_s *context, uint32_t unique)
{
    struct service_context_s *root=get_root_context_workspace(workspace);

    context=get_next_service_context(root, context, "workspace");

    while (context) {

	if (get_workspace_mount_ctx(context)==workspace && 
	(context->type==SERVICE_CTX_TYPE_BROWSE && (unique==0 || context->service.browse.unique==unique))) break;
	context=get_next_service_context(root, context, "workspace");

    }

    return context;

}

struct service_context_s *create_network_browse_context(struct workspace_mount_s *w, struct service_context_s *parent, unsigned int ctxflags, unsigned int type, uint32_t unique)
{
    struct service_context_s *context=NULL;

    logoutput("create_network_browse_context: type %i", type);

    context=create_service_context(w, parent, NULL, SERVICE_CTX_TYPE_BROWSE, NULL);

    if (context) {

	context->service.browse.type=type;
	context->service.browse.unique=unique;
	context->flags |= ctxflags;

	set_context_filesystem_workspace(context);
	set_name_service_context(context);

    }

    return context;

}

struct entry_s *create_network_map_entry(struct service_context_s *context, struct directory_s *directory, struct name_s *xname, unsigned int *error)
{
    struct create_entry_s ce;
    struct stat st;

    /* stat values for a network map */

    st.st_mode=S_IFDIR | S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    st.st_uid=0;
    st.st_gid=0;
    st.st_ino=0;
    st.st_dev=0;
    st.st_nlink=2;
    st.st_rdev=0;
    st.st_size=_INODE_DIRECTORY_SIZE;
    st.st_blksize=1024;
    st.st_blocks=(unsigned int) (st.st_size / st.st_blksize) + ((st.st_size % st.st_blksize)==0) ? 1 : 0;

    get_current_time(&st.st_mtim);
    memcpy(&st.st_ctim, &st.st_mtim, sizeof(struct timespec));
    memcpy(&st.st_atim, &st.st_mtim, sizeof(struct timespec));

    init_create_entry(&ce, xname, NULL, directory, NULL, context, &st, NULL);
    return create_entry_extended_batch(&ce);

}

struct entry_s *install_virtualnetwork_map(struct service_context_s *context, struct entry_s *parent, char *name, const char *what, unsigned char *p_action)
{
    struct entry_s *entry=NULL;
    unsigned int error=0;
    struct directory_s *directory01=get_directory(parent->inode);
    struct simple_lock_s wlock01;

    if (wlock_directory(directory01, &wlock01)==0) {
	struct name_s xname;
	struct directory_s *directory02=NULL;
	struct simple_lock_s wlock02;
	struct inode_s *inode=NULL;

	xname.name=name;
	xname.len=strlen(name);
	calculate_nameindex(&xname);

	entry=find_entry_batch(directory01, &xname, &error);

	/* only install if not exists */

	if (entry) {

	    logoutput_info("install_virtualnetwork_map: map %s already exists", name);
	    unlock_directory(directory01, &wlock01);
	    if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_FOUND;
	    goto out;

	} else {

	    logoutput_info("install_virtualnetwork_map: map %s not found: continue", name);

	}

	error=0;
	entry=create_network_map_entry(context, directory01, &xname, &error);

	if (entry==NULL) {

	    logoutput_warning("install_virtualnetwork_map: unable to create map %s", name);
	    unlock_directory(directory01, &wlock01);
	    if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_ERROR;
	    goto out;

	}

	logoutput_info("install_virtualnetwork_map: created map %s for %s", name, ((what) ? what : "unknown"));
	if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_ADDED;
	inode=entry->inode;
	directory02=get_directory(inode);

	if (wlock_directory(directory02, &wlock02)==0) {
	    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

	    logoutput_info("install_virtualnetwork_map: create special files and set fs");

	    if (strcmp(what, "network")==0) {

		if (fs_options.network.network_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_network_desktopentry_file(entry, workspace);

		use_browse_fs(inode);

	    } else if (strcmp(what, "domain")==0 || strcmp(what, "netgroup")==0) {

		if (fs_options.network.domain_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_netgroup_desktopentry_file(entry, workspace);

		use_browse_fs(inode);

	    } else if (strcmp(what, "server")==0 || strcmp(what, "nethost")==0) {

		if (fs_options.network.server_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_netserver_desktopentry_file(entry, workspace);

		use_browse_fs(inode);

	    } else if (strcmp(what, "share")==0 || strcmp(what, "netshare")==0) {

		if (fs_options.network.share_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_netshare_desktopentry_file(entry, workspace);

		use_service_path_fs(inode);

	    }

	    unlock_directory(directory02, &wlock02);

	}

	unlock_directory(directory01, &wlock01);

    } else {

	if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_ERROR;

    }

    out:
    return entry;

}

char *get_network_name(unsigned int flag)
{

    if (flag==SERVICE_CTX_FLAG_SFTP) {

	return (fs_options.sftp.network_name) ? fs_options.sftp.network_name : _OPTIONS_SFTP_NETWORK_NAME_DEFAULT;

    } else if (flag==SERVICE_CTX_FLAG_NFS) {

	return (fs_options.nfs.network_name) ? fs_options.nfs.network_name : _OPTIONS_NFS_NETWORK_NAME_DEFAULT;

    }

    return NULL;
}

unsigned int get_network_flag(struct name_s *xname)
{
    char *name=NULL;
    unsigned int flag=0;

    name=get_network_name(SERVICE_CTX_FLAG_SFTP);

    if (strlen(name)==xname->len && memcmp(name, xname->name, xname->len)==0) return SERVICE_CTX_FLAG_SFTP;

    name=get_network_name(SERVICE_CTX_FLAG_NFS);

    if (strlen(name)==xname->len && memcmp(name, xname->name, xname->len)==0) return SERVICE_CTX_FLAG_NFS;

    return flag;
}

unsigned int get_context_network_flag(struct service_context_s *context)
{
    unsigned int flag=context->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND;

    while (flag==0) {

	context=get_parent_context(context);
	if (context==NULL) break;
	flag=context->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND;

    }

    return flag;
}

#define B_STRCPY_FLAG_NO_TRUNCATE				1
#define B_STRCPY_FLAG_NO_OVERWRITE				2

static size_t boundary_strcpy(void *data, unsigned char type, char *buffer, size_t size, unsigned int flags)
{
    unsigned int len=0;
    char *name=NULL;
    size_t result=0;

    if (type=='c') {

	name=(char *) data;
	len=strlen(name);

    } else if (type=='s') {
	struct ssh_string_s *tmp=(struct ssh_string_s *) data;

	name=tmp->ptr;
	len=tmp->len;

    } else {

	logoutput_error("boundary_strcpy: type %i not supported", type);
	return 0;

    }

    if (len>=size) {

	if (flags & B_STRCPY_FLAG_NO_TRUNCATE) {

	    result=0;

	} else {

	    memcpy(buffer, name, size-1);
	    buffer[size-1]='\0';
	    result=size-1;

	}

    } else {

	memcpy(buffer, name, len+1);
	result=len;

    }

    return result;
}

static void read_hostname_domainname(char *name, char *hostname, size_t lenh, char *domain, size_t lend)
{
    char *sep=strchr(name, '.');

    if (sep) {
	unsigned int len=(unsigned int) (sep - name);
	struct ssh_string_s tmp=SSH_STRING_SET(len, name);

	boundary_strcpy(&tmp, 's', hostname, lenh, B_STRCPY_FLAG_NO_TRUNCATE);
	if (domain) boundary_strcpy(sep+1, 'c', domain, lend, B_STRCPY_FLAG_NO_TRUNCATE);

    } else {

	boundary_strcpy(name, 'c', hostname, lenh, B_STRCPY_FLAG_NO_TRUNCATE);

    }

}

static unsigned int check_create_remote_service_ssh(struct service_context_s *context, char *fullname, void *ptr)
{
    unsigned int len1=strlen("ssh-channel:sftp:");
    unsigned int len2=0;
    unsigned int count=0;

    if (fullname==NULL) fullname="ssh-channel:sftp:home";
    len2=strlen(fullname);
    if (len2<=len1) return 0;

    if (memcmp(fullname, "ssh-channel:sftp:", len1)==0) {
	char name[len2];
	struct sftp_service_s service;

	memset(name, 0, len2);
	strcpy(name, &fullname[len1]);
	len2=strlen(name);

	replace_cntrl_char(name, len2, REPLACE_CNTRL_FLAG_TEXT);
	if (skip_heading_spaces(name, len2)>0) len2=strlen(name);
	if (skip_trailing_spaces(name, len2, SKIPSPACE_FLAG_REPLACEBYZERO)>0) len2=strlen(name);

	memset(&service, 0, sizeof(struct sftp_service_s));

	service.fullname=fullname;
	service.name=name;
	service.prefix=NULL;
	service.uri=NULL;

	if (add_shared_map_sftp(context, &service, ptr)) count++;

    }

    return count;

}

static void determine_fqdn(struct service_context_s *hostctx, int fd, struct discover_resource_s *nethost)
{
    struct discover_resource_s *netgroup=get_discover_netgroup(nethost);
    char *domainname=(strlen(netgroup->service.group.name)==0) ? netgroup->service.group.name : NULL;
    char *hostname=(strlen(nethost->service.host.address.hostname)>0) ? nethost->service.host.address.hostname : NULL;

    if (nethost->service.host.lookupname.type==0 || nethost->service.host.lookupname.type==LOOKUP_NAME_TYPE_DISCOVERNAME) {
	struct host_address_s *host=&nethost->service.host.address;
	char *ip=((host->ip.family==IP_ADDRESS_FAMILY_IPv6) ? host->ip.ip.v6 : host->ip.ip.v4);
	char *name=lookupname_dns(ip);

	if (name) {

	    nethost->service.host.lookupname.type=LOOKUP_NAME_TYPE_DNSNAME;
	    nethost->service.host.lookupname.name.dnsname=name;
	    return;

	} else {
	    struct fs_connection_s *conn=get_fs_connection_ssh_interface(&hostctx->interface);

	    name=get_connection_hostname(conn, fd, 1, NULL);

	    if (name) {

		nethost->service.host.lookupname.type=LOOKUP_NAME_TYPE_CANONNAME;
		nethost->service.host.lookupname.name.canonname=name;
		return;

	    }

	}

    }

    return;

}

static void update_service_ctx_refresh(struct service_context_s *ctx, struct timespec *changed)
{

    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {
	struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;

	init_service_ctx_lock(&ctxlock, NULL, ctx);

	/* get write access to the context */

	if (lock_service_context(&ctxlock, "w", "c")) {
	    struct timespec *refresh=&ctx->service.browse.refresh;

	    if (changed) {

		if (refresh->tv_sec < changed->tv_sec || ((refresh->tv_sec == changed->tv_sec) && (refresh->tv_nsec < changed->tv_nsec))) {

		    memcpy(&ctx->service.browse.refresh, changed, sizeof(struct timespec));

		}

	    } else {

		get_current_time(&ctx->service.browse.refresh);

	    }

	    unlock_service_context(&ctxlock, "w", "c");

	}

    }

}

static struct service_context_s *connect_resource_via_host(struct discover_resource_s *resource, struct discover_service_s *discover, unsigned int flag)
{
    struct discover_resource_s *nethost=get_discover_nethost(resource);
    struct discover_resource_s *netgroup=NULL;
    struct service_context_s *hostctx=NULL;
    struct context_interface_s *interface=NULL;
    struct service_context_s *networkctx=NULL;
    struct workspace_mount_s *workspace=NULL;
    struct service_context_s *parentctx=NULL;
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;
    char hostname[HOST_HOSTNAME_MAX_LENGTH + 1];
    char domainbuffer[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
    char *domainname=NULL;
    uid_t uid=(uid_t) -1;
    struct host_address_s *host=&nethost->service.host.address;
    struct network_port_s *port=&resource->service.socket.port;
    struct service_address_s service;
    char *name=NULL;
    int fd=-1;

    logoutput("connect_resource_via_host");

    networkctx=discover->networkctx;
    parentctx=networkctx;
    workspace=get_workspace_mount_ctx(networkctx);
    uid=workspace->user->pwd.pw_uid;

    name=(strlen(host->hostname)>0 ? host->hostname : ((host->ip.family==IP_ADDRESS_FAMILY_IPv6) ? host->ip.ip.v6 : host->ip.ip.v4));

    init_service_ctx_lock(&ctxlock, NULL, NULL);
    set_root_service_ctx_lock(get_root_context(networkctx), &ctxlock);
    lock_service_context(&ctxlock, "w", "w");

    hostctx=get_next_network_browse_context(workspace, NULL, nethost->unique);

    if (hostctx) {

	/* context for this resource does already exist for this workspace: done ..
	    TODO: look for refresh/reconnect when failed before
	    for now skip
	    20210429 */

	logoutput("connect_resource_via_host: host context does already exist for %s:%s:%i", ((port->type==_NETWORK_PORT_UDP) ? "udp" : "tcp"), name, port->nr);

    } else {
	unsigned int itype=0;

	if (flag & DISCOVER_RESOURCE_FLAG_SSH) {

	    itype=_INTERFACE_TYPE_SSH_SESSION;

	} else if (flag & DISCOVER_RESOURCE_FLAG_SMB) {

	    itype=_INTERFACE_TYPE_SMB_SERVER;

	/* more inerfaces like NFS, WEBDAV, GIT .. */

	}

	if (itype>0) {
	    struct interface_list_s *ilist=get_interface_list(discover->ailist, discover->count, itype);

	    if (ilist==NULL) {

		logoutput_warning("connect_resource_via_host: interface operations not found for this network");
		unlock_service_context(&ctxlock, "w", "w");
		goto error;

	    }

	    hostctx=create_ssh_server_service_context(networkctx, ilist, nethost->unique);

	    if (hostctx==NULL) {

		logoutput("connect_resource_via_host: unable to create service context for %s:%s:%i", ((port->type==_NETWORK_PORT_UDP) ? "udp" : "tcp"), name, port->nr);
		unlock_service_context(&ctxlock, "w", "w");
		goto error;

	    }

	    logoutput("connect_resource_via_host: created service context for %s:%s:%i", ((port->type==_NETWORK_PORT_UDP) ? "udp" : "tcp"), name, port->nr);

	} else {

	    logoutput("connect_resource_via_host: network type not supported");
	    unlock_service_context(&ctxlock, "w", "w");
	    goto error;

	}

    }

    unlock_service_context(&ctxlock, "w", "w");

    update_service_ctx_refresh(hostctx, &nethost->changed);

    /* here connect and start the interface/backend */

    memset(&service, 0, sizeof(struct service_address_s));
    service.type=_SERVICE_TYPE_PORT;
    memcpy(&service.target.port, port, sizeof(struct network_port_s));

    interface=&hostctx->interface;

    fd=(* interface->connect)(uid, interface, host, &service);

    if (fd==-1) {

	logoutput_warning("connect_resource_via_host: unable not to connect to %s:%s:%i", ((port->type==_NETWORK_PORT_UDP) ? "udp" : "tcp"), name, port->nr);
	goto error;

    } else {

	logoutput("connect_resource_via_host: connected to %s:%s:%i", ((port->type==_NETWORK_PORT_UDP) ? "udp" : "tcp"), name, port->nr);

    }

    if ((* interface->start)(interface, fd, NULL)==0) {

	logoutput("connect_resource_via_host: service started");

    } else {

	logoutput("connect_resource_via_host: unable to start service");
	goto error;

    }

    determine_fqdn(hostctx, fd, nethost);
    if (nethost->service.host.lookupname.type==0) goto error;

    memset(hostname, 0, HOST_HOSTNAME_MAX_LENGTH + 1);
    memset(domainbuffer, 0, HOST_HOSTNAME_MAX_LENGTH + 1);

    if (nethost->flags & DISCOVER_RESOURCE_FLAG_NODOMAIN) domainname=&domainbuffer[0];

    switch (nethost->service.host.lookupname.type) {

	case LOOKUP_NAME_TYPE_CANONNAME:

	    name=nethost->service.host.lookupname.name.canonname;
	    break;

	case LOOKUP_NAME_TYPE_DNSNAME:

	    name=nethost->service.host.lookupname.name.dnsname;
	    break;

	case LOOKUP_NAME_TYPE_DISCOVERNAME:

	    name=nethost->service.host.lookupname.name.discovername;
	    break;

    }

    if (name && strlen(name)>0) {

	read_hostname_domainname(name, hostname, HOST_HOSTNAME_FQDN_MAX_LENGTH, domainname, HOST_HOSTNAME_FQDN_MAX_LENGTH);

    } else {

	/* TODO: here maybe ask the remote backend ask it's name ... with ssh it's possible to get the remote hostname easily */
	goto error;

    }

    if (nethost->flags & DISCOVER_RESOURCE_FLAG_NODOMAIN) {

	if (domainname==NULL || strlen(domainname)==0) {

	    logoutput_warning("connect_resource_via_host: domainname for hostname %s not found/set", hostname);
	    goto error;

	}

	logoutput("connect_resource_via_host: using hostname %s and domain %s", hostname, domainname);

	netgroup=check_create_netgroup_resource(domainname);
	if (netgroup==NULL) goto error;

	/* make nethost's parent this netgroup */
	remove_list_element(&nethost->list);
	add_list_element_first(&netgroup->service.group.header, &nethost->list);
	nethost->flags &= ~DISCOVER_RESOURCE_FLAG_NODOMAIN;

    } else {

	netgroup=get_discover_netgroup(nethost);

    }

    init_service_ctx_lock(&ctxlock, parentctx, NULL);

    if (lock_service_context(&ctxlock, "w", "p")==1) {
	struct service_context_s *domainctx=NULL;

	domainctx=get_next_network_browse_context(workspace, NULL, netgroup->unique);

	if (domainctx==NULL) {

	    domainctx=create_network_browse_context(workspace, networkctx, (networkctx->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND), SERVICE_BROWSE_TYPE_NETGROUP, netgroup->unique);
	    if (domainctx==NULL) {

		unlock_service_context(&ctxlock, "w", "p");
		goto error;

	    }

	    set_parent_service_context_unlocked(parentctx, domainctx);

	}

	unlock_service_context(&ctxlock, "w", "p");
	parentctx=domainctx;
	update_service_ctx_refresh(domainctx, &netgroup->changed);

    }

    /* add to tree only when not done before */
    set_parent_service_context(parentctx, hostctx);
    return hostctx;

    error:

    logoutput("connect_resource_via_host: error ... cannot continue");
    /* additional actions like close and free if hostctx is defined */
    return NULL;
}

/* function which finds all domain/host/socket combinations in the cache of resources of a particular
    type (SFTP, SMB, NFS, WEBDAV ir .... ) and connects to them by calling the standard functions connect and start
    for the interface 
    this functionm is called when an user enters the network of a particular type and the map has not been populated before 
    (refresh also ??)
    */

static void network_service_context_connect_thread(void *ptr)
{
    struct discover_service_s *discover=(struct discover_service_s *) ptr;
    struct service_context_s *networkctx=NULL;
    struct workspace_mount_s *workspace=NULL;
    struct discover_resource_s *resource=NULL;
    unsigned int ctxflag=0;
    unsigned int flag=0;
    unsigned int count=build_interface_ops_list(NULL, NULL, 0);
    struct interface_list_s ailist[count];
    struct interface_list_s *ilist=NULL;
    struct simple_lock_s rlock;
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;
    unsigned int hashvalue=0;
    void *index=NULL;

    networkctx=discover->networkctx;
    if (networkctx==NULL) goto out;
    ctxflag=(networkctx->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND); 

    logoutput("network_service_context_connect_thread: ctxflag %i", ctxflag);

    init_service_ctx_lock(&ctxlock, NULL, networkctx);

    startlock:

    if (lock_service_context(&ctxlock, "w", "c")==1) {
	struct timespec *changed=get_discover_network_changed(0, 0);
	struct timespec *refresh=&networkctx->service.browse.refresh;

	if (refresh->tv_sec < changed->tv_sec || ((refresh->tv_sec == changed->tv_sec) && (refresh->tv_nsec < changed->tv_nsec))) {

	    networkctx->service.browse.threadid=pthread_self();

	} else {

	    unlock_service_context(&ctxlock, "w", "c");
	    return;

	}

	unlock_service_context(&ctxlock, "w", "c");

    }

    workspace=get_workspace_mount_ctx(networkctx);
    discover->count=build_interface_ops_list(NULL, ailist, 0);
    discover->ailist=ailist;

    /* on what network?
	- smb : 		Windows Network
	- sftp over ssh :	Open Secure Network
	- webdav		Internet Directories
	- nfs			Network File System
	- git ???
	- */

    if (ctxflag & SERVICE_CTX_FLAG_SFTP) {

	/* look for sftp resources but also ssh resources: they can provide the sftp service as subsystem */

	flag=DISCOVER_RESOURCE_FLAG_SFTP | DISCOVER_RESOURCE_FLAG_SSH;

    } else if (ctxflag & SERVICE_CTX_FLAG_SMB) {

	flag=DISCOVER_RESOURCE_FLAG_SMB;

    } else if (ctxflag & SERVICE_CTX_FLAG_NFS) {

	flag=DISCOVER_RESOURCE_FLAG_NFS;

    }

    if (flag==0) goto out;

    /* look for the resources */

    init_service_ctx_lock(&ctxlock, NULL, NULL);
    set_root_service_ctx_lock(get_root_context(networkctx), &ctxlock);

    rlock_discover_network(&rlock);
    resource=get_next_hashed_discover_resource(&index, &hashvalue);

    while (resource) {
	struct service_context_s *socketctx=NULL;
	struct service_context_s *hostctx=NULL;

	/* select only those resources which fit in this network:
	    hosts providing a smb service in a Windows Network
	    hosts providing a nfs service in a Network File System
	    but howto deal with services encapsulated in another like SFTP over SSH? 
	    Avahi provides some info about that like the type _sftp_ssh._tcp which says sftp over ssh.
	    but actually you have to try the service first and than it can provide info about
	    the services it offers....
	    any information before is welcome! so you don't have to connect first...
	*/

	if ((resource->flags & flag)==0 || resource->type != DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) goto next;
	logoutput("network_service_context_connect_thread: found socket %i flags %i", resource->service.socket.port, resource->flags);

	lock_service_context(&ctxlock, "w", "w");

	socketctx=get_next_network_browse_context(workspace, NULL, resource->unique);

	/* context for this resource does already exist for this workspace: done ..
	    TODO: look for refresh/reconnect when failed before
		for now skip 
		20210429 */

	if (socketctx) {

	    unlock_service_context(&ctxlock, "w", "w");
	    goto next;

	}

	socketctx=create_network_browse_context(workspace, NULL, ctxflag, SERVICE_BROWSE_TYPE_NETSOCKET, resource->unique);

	if (socketctx==NULL) {

	    logoutput_warning("network_service_context_connect_thread: unable to create virtual service context");
	    unlock_service_context(&ctxlock, "w", "w");
	    goto next;

	}

	unlock_service_context(&ctxlock, "w", "w");

	/* sftp over ssh goes via a connection to the server/host,
	    so connect to the host first and then determine the sftp services
	    also:
	    nfs
	    smb
	    webdav
	*/

	if (flag & DISCOVER_RESOURCE_FLAG_SSH) hostctx=connect_resource_via_host(resource, discover, flag);

	/* run the cb on the created dentry and connection context to detect the shared directories for example

	    TODO:

	    -	also discovery of other shared services and/or devices on the server like printer, scanner */

	if (hostctx) {

	    set_parent_service_context(hostctx, socketctx);

	    if (hostctx->interface.type==_INTERFACE_TYPE_SSH_SESSION) {

		if (get_remote_services_ssh_server(hostctx, check_create_remote_service_ssh, discover)==0) {

		    /* no services added: take the default */

		    check_create_remote_service_ssh(hostctx, NULL, discover);


		}

	    }

	}

	logoutput_debug("network_service_context_connect_thread: next (hashvalue=%i)", hashvalue);

	next:
	resource=get_next_hashed_discover_resource(&index, &hashvalue);

    }

    logoutput_debug("network_service_context_connect_thread: unlock discover");
    unlock_discover_network(&rlock);

    out:

    init_service_ctx_lock(&ctxlock, NULL, networkctx);

    if (lock_service_context(&ctxlock, "w", "c")==1) {
	struct timespec *changed=get_discover_network_changed(0, 0);
	struct timespec *refresh=&networkctx->service.browse.refresh;

	networkctx->service.browse.threadid=0;
	refresh->tv_sec=changed->tv_sec;
	refresh->tv_nsec=changed->tv_nsec;

	unlock_service_context(&ctxlock, "w", "c");

    }

    free(discover);

}

static void start_discover_service_thread(struct service_context_s *ctx)
{
    struct discover_service_s *discover=malloc(sizeof(struct discover_service_s));

    if (discover) {

	memset(discover, 0, sizeof(struct discover_service_s));
	discover->networkctx=ctx;
	logoutput("start_service_context_connect: start threat");
	work_workerthread(NULL, 0, network_service_context_connect_thread, (void *) discover, NULL);

    } else {

	logoutput_warning("start_service_context_connect: unable to allocate memory");

    }

}

void start_discover_service_context_connect(struct workspace_mount_s *workspace)
{
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;
    struct service_context_s *root=get_root_context_workspace(workspace);

    logoutput("start_discover_service_context_connect: found root %s type %i", root->name, root->type);

    init_service_ctx_lock(&ctxlock, root, NULL);

    if (lock_service_context(&ctxlock, "r", "p")==1) {
	struct service_context_s *ctx=get_next_service_context(root, NULL, "tree");

	while (ctx) {

	    logoutput("start_discover_service_context_connect: found ctx %s type %i", ctx->name, ctx->type);

	    if (ctx->type==SERVICE_CTX_TYPE_BROWSE && ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) {

		set_ctx_service_ctx_lock(ctx, &ctxlock);

		if (lock_service_context(&ctxlock, "w", "c")==1) {
		    struct timespec *changed=get_discover_network_changed(0, 0);
		    struct timespec *refresh=&ctx->service.browse.refresh;
		    unsigned char do_discover=0;

		    logoutput("start_discover_service_context_connect: thread id %i compare refresh %li:%i to changed %li:%i", ctx->service.browse.threadid, refresh->tv_sec, refresh->tv_nsec, changed->tv_sec, changed->tv_nsec);

		    if (ctx->service.browse.threadid==0 && (refresh->tv_sec < changed->tv_sec || ((refresh->tv_sec == changed->tv_sec) && (refresh->tv_nsec < changed->tv_nsec))))
			do_discover=1;

		    unlock_service_context(&ctxlock, "w", "c");

		    if (do_discover) start_discover_service_thread(ctx);

		}

	    }

	    nextctx:
	    ctx=get_next_service_context(root, ctx, "tree");

	}

	unlock_service_context(&ctxlock, "r", "p");

    }

}

void populate_network_workspace_mount(struct workspace_mount_s *workspace)
{
    struct service_context_s *root=get_root_context_workspace(workspace);
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;

    init_service_ctx_lock(&ctxlock, root, NULL);

    if (lock_service_context(&ctxlock, "w", "p")==1) {

	/* SFTP network*/

	if (fs_options.network.services & _OPTIONS_NETWORK_ENABLE_SFTP) {
	    char *name=get_network_name(SERVICE_CTX_FLAG_SFTP);

	    if (name) {
		struct service_context_s *ctx=create_network_browse_context(workspace, root, SERVICE_CTX_FLAG_SFTP, SERVICE_BROWSE_TYPE_NETWORK, 0);

		if (ctx) {

		    set_parent_service_context_unlocked(root, ctx);
		    logoutput("populate_network_workspace_mount: add network %s", name);

		} else {

		    logoutput("populate_network_workspace_mount: unable to add network %s", name);

		}

	    } else {

		logoutput("populate_network_workspace_mount: name not defined for SFTP network");

	    }

	}

	/* NFS network */

	if (fs_options.network.services & _OPTIONS_NETWORK_ENABLE_NFS) {
	    char *name=get_network_name(SERVICE_CTX_FLAG_NFS);

	    if (name) {
		struct service_context_s *ctx=create_network_browse_context(workspace, root, SERVICE_CTX_FLAG_NFS, SERVICE_BROWSE_TYPE_NETWORK, 0);

		if (ctx) {

		    set_parent_service_context_unlocked(root, ctx);
		    logoutput("populate_network_workspace_mount: add network %s", name);

		} else {

		    logoutput("populate_network_workspace_mount: unable to add network %s", name);

		}

	    } else {

		logoutput("populate_network_workspace_mount: name not defined for NFS network");

	    }

	}

	/* SMB network */

	if (fs_options.network.services & _OPTIONS_NETWORK_ENABLE_SMB) {
	    char *name=get_network_name(SERVICE_CTX_FLAG_SMB);

	    if (name) {
		struct service_context_s *ctx=create_network_browse_context(workspace, root, SERVICE_CTX_FLAG_SMB, SERVICE_BROWSE_TYPE_NETWORK, 0);

		if (ctx) {

		    set_parent_service_context_unlocked(root, ctx);
		    logoutput("populate_network_workspace_mount: add network %s", name);

		} else {

		    logoutput("populate_network_workspace_mount: unable to add network %s", name);

		}

	    } else {

		logoutput("populate_network_workspace_mount: name not defined for SMB network");

	    }

	}

	unlock_service_context(&ctxlock, "w", "p");

    }

}

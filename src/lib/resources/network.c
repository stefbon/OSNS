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

#include <arpa/inet.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"

#include "resource.h"
#include "localhost.h"
#include "network.h"
#include "create.h"

struct resource_network_root_s {
    unsigned int						status;
    struct system_timespec_s					changed;
    void							*ptr;
    struct list_header_s					header;
};

struct resource_network_group_s {
    unsigned int						flags;
    struct list_element_s					list;
    struct list_header_s					header;
    char							name[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
};

struct resource_network_host_s {
    unsigned int						flags;
    struct list_element_s					list;
    struct list_header_s					header;
    struct host_address_s					address;
};

struct resource_network_socket_s {
    unsigned int						flags;
    struct list_element_s					list;
    struct network_service_s					service;
};

struct resource_network_common_s {
    unsigned int						flags;
    struct list_element_s					list;
    char							buffer[];
};

static struct resource_subsys_s network_subsys;
static struct resource_s *rnetwork=NULL;
static struct resource_s *rnodomain=NULL;

static void init_network_resource(struct resource_s *r)
{

    logoutput_debug("init_network_resource: name %s", r->name);

    if (r) {

	if (strcmp(r->name, "network")==0) {
	    struct resource_network_root_s *root=(struct resource_network_root_s *) r->buffer;

	    root->status=0;
	    set_system_time(&root->changed, 0, 0);
	    root->ptr=NULL;
	    init_list_header(&root->header, SIMPLE_LIST_TYPE_EMPTY, NULL);

	} else if (strcmp(r->name, "network-group")==0) {
	    struct resource_network_group_s *group=(struct resource_network_group_s *) r->buffer;

	    group->flags=0;
	    init_list_element(&group->list, NULL);
	    init_list_header(&group->header, SIMPLE_LIST_TYPE_EMPTY, NULL);

	} else if (strcmp(r->name, "network-host")==0) {
	    struct resource_network_host_s *host=(struct resource_network_host_s *) r->buffer;

	    host->flags=0;
	    init_list_element(&host->list, NULL);
	    init_list_header(&host->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	    init_host_address(&host->address);

	} else if (strcmp(r->name, "network-socket")==0) {
	    struct resource_network_socket_s *socket=(struct resource_network_socket_s *) r->buffer;

	    init_list_element(&socket->list, NULL);
	    init_network_service(&socket->service);

	} else {

	    logoutput_warning("init_network_resource: name %s not reckognized");

	}

    }

}

static void free_network_resource(struct resource_s *r)
{
    free(r);
}

static unsigned int get_size(struct resource_subsys_s *rss, const char *name)
{
    unsigned int size=0;

    if ((rss==&network_subsys) && name) {

	if (strcmp(name, "network")==0) {

	    size=sizeof(struct resource_network_root_s);

	} else if (strcmp(name, "network-group")==0) {

	    size=sizeof(struct resource_network_group_s);

	} else if (strcmp(name, "network-host")==0) {

	    size=sizeof(struct resource_network_host_s);

	} else if (strcmp(name, "network-socket")==0) {

	    size=sizeof(struct resource_network_socket_s);

	}

    }

    return size;
}

static void process_action_default(struct resource_s *r, unsigned char what, void *ptr)
{
    char *action="--unknown--";

    logoutput_debug("cb_discover_default");

    if (r==NULL || (r->subsys != &network_subsys) || r->name==NULL) return;

    switch (what) {

	case RESOURCE_ACTION_ADD:

	    action="added";
	    break;

	case RESOURCE_ACTION_RM:

	    action="removed";
	    break;

	case RESOURCE_ACTION_CHANGE:

	    action="changed";
	    break;

	default:

	    logoutput_warning("process_action_default: action code %u not reckognized", what);

    }

    if (strcmp(r->name, "network-group")==0) {
	struct resource_network_group_s *group=(struct resource_network_group_s *) r->buffer;

	logoutput_debug("process_action_default: %s network group %s", action, group->name);

    } else if (strcmp(r->name, "network-host")==0) {
	struct resource_network_host_s *host=(struct resource_network_host_s *) r->buffer;
	struct host_address_s *address=&host->address;
	char *name=NULL;

	if (address->flags & HOST_ADDRESS_FLAG_HOSTNAME) {

	    name=address->hostname;

	} else if (address->flags & HOST_ADDRESS_FLAG_IP) {

	    if (address->ip.family==IP_ADDRESS_FAMILY_IPv4) {

		name=address->ip.addr.v4;

	    } else if (address->ip.family==IP_ADDRESS_FAMILY_IPv6) {

		name=address->ip.addr.v6;

	    }

	}

	logoutput_debug("process_action_default: %s network host %s", action, (name ? name : "--unknown--"));

    } else if (strcmp(r->name, "network-socket")==0) {
	struct resource_network_socket_s *socket=(struct resource_network_socket_s *) r->buffer;
	char *type="--unknown--";

	switch (socket->service.port.type) {

	    case _NETWORK_PORT_TCP:

		type="tcp";
		break;

	    case _NETWORK_PORT_UDP:

		type="udp";
		break;

	}

	logoutput_debug("process_action_default: %s network socket %u-%s:%i", action, socket->service.port.type, type, socket->service.port.nr);

    }

}

static struct resource_subsys_s network_subsys = {
    .name				= "network",
    .status				= 0,
    .init				= init_network_resource,
    .free				= free_network_resource,
    .get_size				= get_size,
    .process_action			= process_action_default,
    .ptr				= NULL,
};

int add_network_subsys(void (* cb)(struct resource_s *r, unsigned char what, void *ptr), void *ptr)
{

    if (rnetwork || rnodomain) return 0;

    rnetwork=create_resource(&network_subsys, "network");
    rnodomain=create_resource(&network_subsys, "network-group");

    if (rnetwork && rnodomain) {
	struct resource_network_root_s *network=(struct resource_network_root_s *) rnetwork->buffer;
	struct resource_network_group_s *group=(struct resource_network_group_s *) rnodomain->buffer;

	add_list_element_first(&network->header, &group->list);
	group->flags |= NETWORK_RESOURCE_FLAG_NODOMAIN;

	rnetwork->unique=get_localhost_unique_ctr();
	add_resource_hash(rnetwork);
	rnodomain->unique=get_localhost_unique_ctr();
	add_resource_hash(rnodomain);

	network_subsys.process_action=(cb ? cb : process_action_default);
	network_subsys.ptr=ptr;

    } else {

	logoutput("add_network_subsys: unable to allocate root and/or nodomain");

	if (rnetwork) {

	    free(rnetwork);
	    rnetwork=NULL;

	}

	if (rnodomain) {

	    free(rnodomain);
	    rnodomain=NULL;

	}

	return -1;

    }

    return 1;

}

static struct list_header_s *get_network_resource_header(struct resource_s *r)
{
    struct list_header_s *h=NULL;

    logoutput_debug("get_network_resource_header: name %s", r->name);

    if ((r->subsys==&network_subsys) && r->name) {

	if (strcmp(r->name, "network")==0) {
	    struct resource_network_root_s *root=(struct resource_network_root_s *) r->buffer;

	    h=&root->header;

	} else if (strcmp(r->name, "network-group")==0) {
	    struct resource_network_group_s *group=(struct resource_network_group_s *) r->buffer;

	    h=&group->header;

	} else if (strcmp(r->name, "network-host")==0) {
	    struct resource_network_host_s *host=(struct resource_network_host_s *) r->buffer;

	    h=&host->header;

	}

    }

    return h;
}

static struct resource_s *get_network_resource_parent(struct resource_s *r)
{
    struct resource_s *parent=NULL;

    if (r) {

	if (strcmp(r->name, "network-group")==0) {
	    struct resource_network_group_s *group=(struct resource_network_group_s *) r->buffer;
	    struct list_header_s *h=group->list.h;

	    if (h) {
		struct resource_network_root_s *root=(struct resource_network_root_s *) ((char *)h - offsetof(struct resource_network_root_s, header));

		parent=(struct resource_s *)((char *) root - offsetof(struct resource_s, buffer));

	    }

	} else if (strcmp(r->name, "network-host")==0) {
	    struct resource_network_host_s *host=(struct resource_network_host_s *) r->buffer;
	    struct list_header_s *h=host->list.h;

	    if (h) {
		struct resource_network_group_s *group=(struct resource_network_group_s *) ((char *)h - offsetof(struct resource_network_group_s, header));

		parent=(struct resource_s *)((char *) group - offsetof(struct resource_s, buffer));

	    }

	} else if (strcmp(r->name, "network-socket")==0) {
	    struct resource_network_socket_s *socket=(struct resource_network_socket_s *) r->buffer;
	    struct list_header_s *h=socket->list.h;

	    if (h) {
		struct resource_network_host_s *host=(struct resource_network_host_s *) ((char *)h - offsetof(struct resource_network_host_s, header));

		parent=(struct resource_s *)((char *) host - offsetof(struct resource_s, buffer));

	    }

	}

    }

    return parent;
}

uint32_t add_network_service_resource(struct host_address_s *address, char *domainname, struct network_port_s *port, unsigned int service, unsigned int transport, unsigned int flags)
{
    char *hostname=NULL;
    char *domain=NULL;
    struct resource_s *rnetgroup=NULL;
    struct resource_s *rnethost=NULL;
    struct resource_s *rnetsocket=NULL;
    unsigned char what=0;
    struct list_header_s *header=NULL;
    struct list_element_s *list=NULL;
    char *target=NULL;

    /* some test on hostname for illegale characters? */

    if ((address->flags & HOST_ADDRESS_FLAG_HOSTNAME)==0) {

	hostname=gethostnamefromspec(address, GETHOSTNAME_FLAG_IGNORE_IP);
	if (hostname) set_host_address(address, hostname, NULL, NULL);

    } else {

	hostname=address->hostname;

    }

    translate_context_host_address(address, &target, NULL);

    if (target) {

	logoutput_debug("add_network_service_resource: target %s:%u flags %u", target, port->nr, flags);

    } else {

	logoutput_debug("add_network_service_resource: no hostname and ip");
	return 0;

    }

    if (domainname && (strlen(domainname)>0)) {

	domain=domainname;

    } else {

	if (hostname) {

	    /* test if hostname has a domain part
	    it's required to exclude the possibility the hostname is an ip address
	    in that case the domain part is nonsense */

	    if (check_family_ip_address(hostname, "ipv4")==0 && check_family_ip_address(hostname, "ipv6")==0) {
		char *sep=strchr(hostname, '.');

		if (sep) {

		    *sep='\0';
		    domain=sep+1;

		}

	    }

	}

    }

    if (domain) {
	unsigned int len=strlen(domain);

	if (len>HOST_HOSTNAME_FQDN_MAX_LENGTH) {

	    logoutput_warning("add_network_service_resource: domain %s too long", domain);
	    return 0;

	}

	logoutput("add_network_service_resource: host %s domain %s", hostname, domain);

	/* find or create the net group (==domain) part */

	header=get_network_resource_header(rnetwork);
	write_lock_list_header(header);

	list=get_list_head(header, 0);

	while (list) {

	    struct resource_network_group_s *group=(struct resource_network_group_s *) ((char *) list - offsetof(struct resource_network_group_s, list));

	    if ((group->flags & NETWORK_RESOURCE_FLAG_NODOMAIN)==0 && strlen(group->name) == len && memcmp(group->name, domain, len)==0) {

		rnetgroup=(struct resource_s *)((char *)group - offsetof(struct resource_s, buffer));
		break;

	    }

	    list=get_next_element(list);

	}

	if (rnetgroup==NULL) {

	    rnetgroup=create_resource(&network_subsys, "network-group");

	    if (rnetgroup) {
		struct resource_network_group_s *group=(struct resource_network_group_s *) rnetgroup->buffer;

		rnetgroup->flags |= flags;
		rnetgroup->unique=get_localhost_unique_ctr();
		add_resource_hash(rnetgroup);

		add_list_element_last(header, &group->list);
		memcpy(group->name, domain, len);
		what=RESOURCE_ACTION_ADD;

	    }

	}

	write_unlock_list_header(header);

	if (rnetgroup==NULL) {

	    logoutput_warning("add_network_service_resource: unable to add domain %s", domain);
	    goto error;

	}

	if (what) {

	    if (what==RESOURCE_ACTION_ADD) set_changed(rnetwork, &rnetgroup->found);
	    (* network_subsys.process_action)(rnetgroup, what, network_subsys.ptr);
	    what=0;

	}

    } else if (hostname) {

	logoutput_debug("add_network_service_resource: host %s (no domain)", hostname);

    } else {

	logoutput_debug("add_network_service_resource: no hostname");

    }

    /* where to add the host: in the list per domain, or the list of hosts without domain */

    if (rnetgroup==NULL) rnetgroup=rnodomain;

    /* find or create the net host */

    header=get_network_resource_header(rnetgroup);
    write_lock_list_header(header);

    list=get_list_head(header, 0);

    while (list) {
	struct resource_network_host_s *host=(struct resource_network_host_s *) ((char *) list - offsetof(struct resource_network_host_s, list));

	if (compare_host_address(&host->address, address)==0) {

	    rnethost=(struct resource_s *)((char *)host - offsetof(struct resource_s, buffer));
	    break;

	}

	list=get_next_element(list);

    }

    if (rnethost==NULL) {

	rnethost=create_resource(&network_subsys, "network-host");

	if (rnethost) {
	    struct resource_network_host_s *host=(struct resource_network_host_s *) rnethost->buffer;

	    rnethost->flags |= flags;
	    rnethost->unique=get_localhost_unique_ctr();
	    add_resource_hash(rnethost);

	    add_list_element_last(header, &host->list);
	    memcpy(&host->address, address, sizeof(struct host_address_s));

	    what=RESOURCE_ACTION_ADD;

	}

    }

    write_unlock_list_header(header);

    if (rnethost==NULL) {

	logoutput_warning("add_network_service_resource: unable to add nethost %s", hostname);
	goto error;

    }

    if (what) {

	if (what==RESOURCE_ACTION_ADD) set_changed(rnetgroup, &rnethost->found);
	(* network_subsys.process_action)(rnethost, what, network_subsys.ptr);
	what=0;

    }

    /*	socket */

    header=get_network_resource_header(rnethost);
    write_lock_list_header(header);

    list=get_list_head(header, 0);

    while (list) {
	struct resource_network_socket_s *socket=(struct resource_network_socket_s *) ((char *) list - offsetof(struct resource_network_socket_s, list));

	if ((socket->service.port.nr==port->nr) && (socket->service.port.type==port->type) && (socket->service.service==service)) {

	    rnetsocket=(struct resource_s *)((char *)socket - offsetof(struct resource_s, buffer));
	    break;

	}

	list=get_next_element(list);

    }

    if (rnetsocket==NULL) {

	rnetsocket=create_resource(&network_subsys, "network-socket");

	if (rnetsocket) {
	    struct resource_network_socket_s *socket=(struct resource_network_socket_s *) rnetsocket->buffer;

	    rnetsocket->flags |= flags;
	    rnetsocket->unique=get_localhost_unique_ctr();
	    add_resource_hash(rnetsocket);

	    add_list_element_last(header, &socket->list);
	    socket->service.port.nr=port->nr;
	    socket->service.port.type=port->type;
	    socket->service.service=service;
	    socket->service.transport=transport;

	    if ((service > 0) && (flags & NETWORK_RESOURCE_FLAG_SERVICE_GUESSED)) socket->flags |= NETWORK_RESOURCE_FLAG_SERVICE_GUESSED;
	    what=RESOURCE_ACTION_ADD;

	}

    }

    write_unlock_list_header(header);

    if (rnetsocket==NULL) {

	logoutput_warning("add_net_service_resource: unable to add netsocket %u:%u", port->type, port->nr);
	goto error;

    }

    if (what) {

	if (what==RESOURCE_ACTION_ADD) set_changed(rnethost, &rnetsocket->found);
	(* network_subsys.process_action)(rnetsocket, what, network_subsys.ptr);
	what=0;

    }


    logoutput("add_network_service_resource: added network port %i to host %s%s%s ip %s", port->nr, hostname, (domain) ? "." : "", (domain) ? domain : "", (address->flags & HOST_ADDRESS_FLAG_IP) ? ((address->ip.family==IP_ADDRESS_FAMILY_IPv4) ? address->ip.addr.v4 : address->ip.addr.v6) : "");
    return (rnetsocket->unique);

    error:

    logoutput_warning("add_network_service_resource: error adding network port %i for %s", port->nr, hostname);
    return 0;

}

void remove_net_service_resource(uint32_t unique)
{
    /* TODO ... */
}

int get_network_resource(uint32_t unique, struct network_resource_s *nr)
{
    struct resource_s *r=NULL;
    int result=0;

    r=lookup_resource_id(unique);

    if (r && (r->subsys==&network_subsys)) {

	if (strcmp(r->name, "network-group")==0 && nr->type==NETWORK_RESOURCE_TYPE_NETWORK_GROUP) {
	    struct resource_network_group_s *group=(struct resource_network_group_s *) r->buffer;
	    unsigned int len=strlen(group->name);
	    struct resource_s *p=get_network_resource_parent(r);

	    memcpy(nr->data.domain, group->name, len);
	    nr->flags = group->flags;
	    result=1;
	    if (p) nr->parent_unique=p->unique;

	} else if (strcmp(r->name, "network-host")==0 && nr->type==NETWORK_RESOURCE_TYPE_NETWORK_HOST) {
	    struct resource_network_host_s *host=(struct resource_network_host_s *) r->buffer;
	    struct host_address_s *address=&nr->data.address;
	    struct resource_s *p=get_network_resource_parent(r);

	    memcpy(address, &host->address, sizeof(struct host_address_s));
	    nr->flags = host->flags;
	    result=1;
	    if (p) nr->parent_unique=p->unique;

	} else if (strcmp(r->name, "network-socket")==0 && nr->type==NETWORK_RESOURCE_TYPE_NETWORK_SOCKET) {
	    struct resource_network_socket_s *socket=(struct resource_network_socket_s *) r->buffer;
	    struct network_service_s *service=&nr->data.service;
	    struct resource_s *p=get_network_resource_parent(r);

	    memcpy(service, &socket->service, sizeof(struct network_service_s));
	    nr->flags = socket->flags;
	    result=1;
	    if (p) nr->parent_unique=p->unique;


	}

    } else {

	result=-1;

    }

    return result;

}

void browse_network_resources(uint32_t unique, void (* cb)(uint32_t unique, const char *name, void *ptr), void *ptr)
{
    struct resource_s *r=NULL;

    r=lookup_resource_id(unique);

    if (r && r->subsys==&network_subsys) {
	struct list_header_s *header=get_network_resource_header(r);
	struct list_element_s *list=NULL;

	logoutput_debug("browse_network_resources: resource %u:%s found", unique, r->name);

	read_lock_list_header(header);
	list=get_list_head(header, 0);

	while (list) {
	    struct resource_network_common_s *common=(struct resource_network_common_s *)((char *) list - offsetof(struct resource_network_common_s, list));
	    struct resource_s *tmp=(struct resource_s *)((char *) common - offsetof(struct resource_s, buffer));

	    (* cb)(tmp->unique, tmp->name, ptr);
	    list=get_next_element(list);

	}

	read_unlock_list_header(header);

    } else {

	if (r) {

	    logoutput_debug("browse_network_resources: resource %u:%s not a network", unique, r->name);

	} else {

	    logoutput_debug("browse_network_resources: no resource found for %u ", unique);

	}

    }

}

uint32_t get_root_network_resources()
{
    uint32_t unique=0;
    if (rnetwork) unique=rnetwork->unique;
    return unique;
}

void browse_every_network_service_resource(unsigned int service, void (* cb)(uint32_t unique, struct network_resource_s *nr, void *ptr), void *ptr)
{
    struct resource_s *r=NULL;

    r=get_next_hashed_resource(NULL, GET_RESOURCE_FLAG_UPDATE_USE);

    while (r) {

	if (r->subsys==&network_subsys) {

	    if (strcmp(r->name, "network-socket")==0) {
		struct resource_network_socket_s *socket=(struct resource_network_socket_s *) r->buffer;

		if (socket->service.service==service) {
		    struct network_resource_s nr;
		    struct resource_s *p=get_network_resource_parent(r);

		    memset(&nr, 0, sizeof(struct network_resource_s));

		    nr.type=NETWORK_RESOURCE_TYPE_NETWORK_SOCKET;
		    if (p) nr.parent_unique=p->unique;
		    memcpy(&nr.data.service, &socket->service, sizeof(struct network_service_s));

		    (* cb)(r->unique, &nr, ptr);

		}

	    }

	}

	r=get_next_hashed_resource(r, GET_RESOURCE_FLAG_UPDATE_USE);

    }

}

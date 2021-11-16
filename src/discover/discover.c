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

#include <inttypes.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "log.h"
#include "misc.h"
#include "main.h"
#include "list.h"
#include "threads.h"
#include "network.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "options.h"
#include "discover.h"

#include "lib/discover.h"

extern struct fs_options_s fs_options;

static struct discover_network_s network;			/* "root" of all domains/hosts/services found */
static struct discover_resource_s nodomain;			/* group with no domain detected */
static struct list_header_s queue=INIT_LIST_HEADER;
static pthread_mutex_t queue_mutex=PTHREAD_MUTEX_INITIALIZER;

struct discover_queue_s {
    unsigned char				method;
    struct list_element_s			list;
    struct host_address_s			host;
    char					*name;
    unsigned int				port;
    unsigned int				fam;
    unsigned int				service;
};

static unsigned int calculate_id_hash(uint32_t unique)
{
    return unique % DISCOVER_NETWORK_HASHSIZE;
}

static unsigned int id_hashfunction(void *data)
{
    struct discover_resource_s *n=(struct discover_resource_s *) data;
    return calculate_id_hash(n->unique);
}

static inline struct hash_element_s *get_hash_element(struct list_element_s *list)
{
    return (struct hash_element_s *) ( ((char *) list) - offsetof(struct hash_element_s, list));
}

static void *get_next_hashed_resource(void **p_index, unsigned int hashvalue)
{
    struct hash_element_s *element=NULL;
    void *index=*p_index;
    struct list_element_s *list=NULL;

    if (index) {

	element=(struct hash_element_s *) index;
	list=get_next_element(&element->list);

    } else {

	hashvalue=hashvalue % DISCOVER_NETWORK_HASHSIZE;
	list=get_list_head(&network.hash[hashvalue], 0);

    }

    element=(list) ? get_hash_element(list) : NULL;
    *p_index=(void *) element;
    return (element) ? element->data : NULL;
}

struct discover_resource_s *lookup_resource_id(uint32_t unique)
{
    unsigned int hashvalue=calculate_id_hash(unique);
    void *index=NULL;
    struct discover_resource_s *resource=NULL;

    resource=(struct discover_resource_s *) get_next_hashed_resource(&index, hashvalue);

    while (resource) {

	if (resource->unique==unique) break;
	resource=(struct discover_resource_s *) get_next_hashed_resource(&index, hashvalue);

    }

    return resource;

}

static void add_resource_hash(struct discover_resource_s *resource)
{
    struct hash_element_s *element=malloc(sizeof(struct hash_element_s));

    if (element) {
	unsigned int hashvalue=calculate_id_hash(resource->unique);

	element->data=(void *) resource;
	init_list_element(&element->list, NULL);

	add_list_element_last(&network.hash[hashvalue], &element->list);

    }

}

static void remove_resource_hash(struct discover_resource_s *resource)
{
    remove_list_element(&resource->list);
}

struct discover_resource_s *get_next_hashed_discover_resource(void **p_index, unsigned int *p_hashvalue)
{
    struct discover_resource_s *resource=NULL;
    unsigned int hashvalue=*p_hashvalue;

    getnext:

    if (hashvalue < DISCOVER_NETWORK_HASHSIZE) {

	resource=(struct discover_resource_s *) get_next_hashed_resource(p_index, hashvalue);

	if (resource==NULL) {

	    hashvalue++;
	    *p_index=NULL;
	    goto getnext;

	}

	*p_hashvalue=hashvalue;

    }

    return resource;

}

static struct discover_resource_s *get_discover_parent(struct discover_resource_s *resource, unsigned char type)
{

    if (resource && resource->type==type) {
	struct list_header_s *header=resource->list.h;

	if (type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {

	    return ((struct discover_resource_s *)((char *) header - offsetof(struct discover_resource_s, service.group.header)));

	} else if (type==DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) {

	    return ((struct discover_resource_s *)((char *) header - offsetof(struct discover_resource_s, service.host.header)));

	}

    }

    return NULL;
}

struct discover_resource_s *get_discover_netgroup(struct discover_resource_s *resource)
{
    return get_discover_parent(resource, DISCOVER_RESOURCE_TYPE_NETWORK_HOST);
}

struct discover_resource_s *get_discover_nethost(struct discover_resource_s *resource)
{
    return get_discover_parent(resource, DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET);
}

static void process_nothing(struct discover_resource_s *n, void *ptr)
{

}

static void add_resource_discover_method(struct discover_resource_s *r, unsigned int type, char *name)
{

    switch (type) {

	case DISCOVER_METHOD_AVAHI:

	    if (r->flags & DISCOVER_RESOURCE_FLAG_AVAHI) return;
	    r->flags |= DISCOVER_RESOURCE_FLAG_AVAHI;
	    break;
	case DISCOVER_METHOD_STATICFILE:
	    if (r->flags & DISCOVER_RESOURCE_FLAG_STATICFILE) return;
	    r->flags |= DISCOVER_RESOURCE_FLAG_STATICFILE;
	    break;

    }

    if (r->type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {
	struct list_element_s *list=NULL;
	struct discover_method_s *method=NULL;

	list=get_list_head(&r->service.host.methods, 0);

	while (list) {

	    method=(struct discover_method_s *) ((char *) list - offsetof(struct discover_method_s, list));
	    if (method->type==type) break;
	    list=get_next_element(list);
	    method=NULL;

	}

	if (method==NULL) {
	    unsigned int len=strlen(name);

	    method=malloc(sizeof(struct discover_method_s) + len);

	    if (method) {

		method->type=type;
		init_list_element(&method->list, NULL);
		method->size=len;
		memcpy(method->buffer, name, len);

		add_list_element_last(&r->service.host.methods, &method->list);

	    }

	}

    }

}

static struct discover_resource_s *create_discover_resource(unsigned char type, unsigned int method)
{
    struct discover_resource_s *r=malloc(sizeof(struct discover_resource_s));

    logoutput_info("create_discover_resource: type %i", type);

    if (r) {

	memset(r, 0, sizeof(struct discover_resource_s));

	r->unique=0;
	r->type=type;
	r->flags=0;
	init_list_element(&r->list, NULL);
	r->found.tv_sec=0;
	r->found.tv_nsec=0;
	r->changed.tv_sec=0;
	r->changed.tv_nsec=0;

	if (type==DISCOVER_RESOURCE_TYPE_NETWORK_GROUP) {

	    init_list_header(&r->service.group.header, SIMPLE_LIST_TYPE_EMPTY, NULL);

	} else if (type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {

	    init_list_header(&r->service.host.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	    init_list_header(&r->service.host.methods, SIMPLE_LIST_TYPE_EMPTY, NULL);
	    init_host_address(&r->service.host.address);
	    r->service.host.lookupname.type=0;

	} else if (type==DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) {

	    init_list_header(&r->service.socket.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	    r->service.socket.refresh.tv_sec=0;
	    r->service.socket.refresh.tv_nsec=0;

	}

    }

    return r;

}

static struct discover_resource_s *create_netgroup_resource(char *name)
{

    struct discover_resource_s *r=create_discover_resource(DISCOVER_RESOURCE_TYPE_NETWORK_GROUP, 0);

    if (r) {
	unsigned int len=strlen(name);

	if (len>HOST_HOSTNAME_FQDN_MAX_LENGTH) len=HOST_HOSTNAME_FQDN_MAX_LENGTH;
	memcpy(r->service.group.name, name, HOST_HOSTNAME_FQDN_MAX_LENGTH);

	r->unique=network.ctr;
	network.ctr++;

	add_list_element_last(&network.header, &r->list);
	add_resource_hash(r);

    }

    return r;

}

struct discover_resource_s *check_create_netgroup_resource(char *name)
{
    struct discover_resource_s *r=NULL;

    pthread_mutex_lock(&queue_mutex);
    r=get_next_discover_resource(NULL, NULL, DISCOVER_RESOURCE_TYPE_NETWORK_GROUP);

    while (r) {
	unsigned int len=strlen(name);

	if (len>HOST_HOSTNAME_FQDN_MAX_LENGTH) len=HOST_HOSTNAME_FQDN_MAX_LENGTH;
	if (memcmp(r->service.group.name, name, len)==0) break;

	r=get_next_discover_resource(NULL, r, DISCOVER_RESOURCE_TYPE_NETWORK_GROUP);

    }

    if (r==NULL) {

	r=create_netgroup_resource(name);
	if (r) logoutput("check_create_netgroup_resource: created group %s with id %i", r->service.group.name, r->unique);

    } else {

	logoutput("check_create_netgroup_resource: found group %s with id %i", r->service.group.name, r->unique);

    }

    pthread_mutex_unlock(&queue_mutex);
    return r;

}

void free_discover_resource(struct discover_resource_s *r)
{

    if (r->type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {

	if (r->service.host.lookupname.type==LOOKUP_NAME_TYPE_CANONNAME) {

	    free(r->service.host.lookupname.name.canonname);
	    r->service.host.lookupname.name.canonname=NULL;

	} else if (r->service.host.lookupname.type==LOOKUP_NAME_TYPE_DNSNAME) {

	    free(r->service.host.lookupname.name.dnsname);
	    r->service.host.lookupname.name.dnsname=NULL;

	}

	r->service.host.lookupname.type=0;

	struct list_element_s *list=get_list_head(&r->service.host.methods, SIMPLE_LIST_FLAG_REMOVE);

	while (list) {

	    struct discover_method_s *method=(struct discover_method_s *) ((char *) list - offsetof(struct discover_method_s, list));

	    free(method);
	    list=get_list_head(&r->service.host.methods, SIMPLE_LIST_FLAG_REMOVE);

	}

    }

    free(r);

}

static void update_single_timespec(struct timespec *changed, struct timespec *r)
{
    if (r->tv_sec < changed->tv_sec || ((r->tv_sec == changed->tv_sec) && r->tv_nsec < changed->tv_nsec)) memcpy(r, changed, sizeof(struct timespec));
}

static void update_changed_times_resources(struct discover_resource_s *r_netsocket)
{
    struct discover_resource_s *r_netgroup=NULL;
    struct discover_resource_s *r_nethost=NULL;
    struct timespec *changed=NULL;

    if (r_netsocket==NULL || r_netsocket->type != DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) return;
    changed=&r_netsocket->changed;

    r_nethost=get_discover_nethost(r_netsocket);
    if (r_nethost==NULL) return;
    update_single_timespec(changed, &r_nethost->changed);

    r_netgroup=get_discover_netgroup(r_nethost);
    if (r_netgroup==NULL) return;
    update_single_timespec(changed, &r_netgroup->changed);

    update_single_timespec(changed, &network.changed);
}

void add_net_service(unsigned int method, char *name, struct host_address_s *host, struct network_port_s *port, unsigned int flag, char *text)
{
    struct list_element_s *list=NULL;
    unsigned int result=0;
    char *hostname=NULL;
    char *domain=NULL;
    unsigned int len=0;
    struct discover_resource_s *resource_netgroup=NULL;
    struct discover_resource_s *resource_nethost=NULL;
    struct discover_resource_s *resource_netsocket=NULL;
    struct list_header_s *header=NULL;

    logoutput("add_net_service: method %i name %s flag %i", method, name, flag);

    hostname=gethostnamefromspec(host, (GETHOSTNAME_FLAG_IGNORE_IPv4 | GETHOSTNAME_FLAG_IGNORE_IPv6));
    if (hostname==NULL) hostname=host->hostname;

    if (method != DISCOVER_METHOD_AVAHI) {

	if (check_family_ip_address(hostname, "ipv4")==0 && check_family_ip_address(hostname, "ipv6")==0) {
	    char *sep=strchr(hostname, '.');

	    if (sep) {

		*sep='\0';
		domain=sep+1;
		len=strlen(domain);
		logoutput("add_net_service: found domain %s", domain);

	    }

	}

    }

    if (domain) {

	/* find the domain */

	header=&network.header;
	list=get_list_head(header, 0);

	while (list) {

	    resource_netgroup=(struct discover_resource_s *)((char *)list - offsetof(struct discover_resource_s, list));

	    if (strlen(resource_netgroup->service.group.name) == len && memcmp(resource_netgroup->service.group.name, domain, len)==0) break;
	    list=get_next_element(list);
	    resource_netgroup=NULL;

	}

	if (resource_netgroup==NULL) {

	    resource_netgroup=create_discover_resource(DISCOVER_RESOURCE_TYPE_NETWORK_GROUP, method);

	    if (resource_netgroup) {

		resource_netgroup->unique=network.ctr;
		network.ctr++;

		get_current_time(&resource_netgroup->found);
		memcpy(&resource_netgroup->changed, &resource_netgroup->found, sizeof(struct timespec));

		strncpy(resource_netgroup->service.group.name, domain, HOST_HOSTNAME_FQDN_MAX_LENGTH);
		add_list_element_last(header, &resource_netgroup->list);
		add_resource_hash(resource_netgroup);

		(* network.process_new_service)(resource_netgroup, network.ptr);

	    } else {

		goto error;

	    }

	}

	resource_netgroup->flags |= flag;
	add_resource_discover_method(resource_netgroup, method, name);

    }

    /* where to add the host: in the list per domain, or the list of hosts without domain */

    header = (resource_netgroup) ? &resource_netgroup->service.group.header : &nodomain.service.group.header;
    list=get_list_head(header, 0);
    len=strlen(hostname);

    while (list) {

	resource_nethost = (struct discover_resource_s *)((char *) list - offsetof(struct discover_resource_s, list));
	if (compare_host_address(&resource_nethost->service.host.address, host)==0) break;
	list=get_next_element(list);
	resource_nethost=NULL;

    }

    if (resource_nethost==NULL) {

	resource_nethost=create_discover_resource(DISCOVER_RESOURCE_TYPE_NETWORK_HOST, method);

	if (resource_nethost) {

	    resource_nethost->unique=network.ctr;
	    network.ctr++;

	    resource_nethost->flags = (resource_netgroup) ? 0 : DISCOVER_RESOURCE_FLAG_NODOMAIN;

	    get_current_time(&resource_nethost->found);
	    memcpy(&resource_nethost->changed, &resource_nethost->found, sizeof(struct timespec));

	    memcpy(&resource_nethost->service.host.address, host, sizeof(struct host_address_s));
	    strcpy(resource_nethost->service.host.address.hostname, hostname);
	    add_list_element_first(header, &resource_nethost->list);
	    add_resource_hash(resource_nethost);

	    resource_nethost->service.host.lookupname.type=LOOKUP_NAME_TYPE_DISCOVERNAME;
	    resource_nethost->service.host.lookupname.name.discovername=resource_nethost->service.host.address.hostname;

	    (* network.process_new_service)(resource_nethost, network.ptr);

	} else {

	    goto error;

	}

    }

    resource_nethost->flags |= flag;
    add_resource_discover_method(resource_nethost, method, name);

    /*	socket */

    header=&resource_nethost->service.host.header;
    list=get_list_head(header, 0);

    while (list) {

	resource_netsocket=(struct discover_resource_s *) ((char *) list - offsetof(struct discover_resource_s, list));
	if (resource_netsocket->service.socket.port.nr==port->nr && resource_netsocket->service.socket.port.type==port->type) break;
	list=get_next_element(list);
	resource_netsocket=NULL;

    }

    if (resource_netsocket==NULL) {

	resource_netsocket=create_discover_resource(DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET, method);

	if (resource_netsocket) {

	    resource_netsocket->unique=network.ctr;
	    network.ctr++;

	    resource_netsocket->service.socket.port.nr=port->nr;
	    resource_netsocket->service.socket.port.type=port->type;
	    get_current_time(&resource_netsocket->found);
	    memcpy(&resource_netsocket->changed, &resource_netsocket->found, sizeof(struct timespec));

	    add_list_element_first(header, &resource_netsocket->list);
	    add_resource_hash(resource_netsocket);

	    if (text) {

		if (strlen(text) > 128) {

		    strncpy(resource_netsocket->service.socket.text, text, 128);

		} else {

		    strcpy(resource_netsocket->service.socket.text, text);

		}

	    }

	    update_changed_times_resources(resource_netsocket);

	    (* network.process_new_service)(resource_netsocket, network.ptr);

	} else {

	    goto error;

	}

    }

    resource_netsocket->flags |= flag;
    add_resource_discover_method(resource_netsocket, method, name);

    logoutput("add_net_service: added network port %i to host %s%s%s ip %s", port->nr, hostname, (domain) ? "." : "", (domain) ? domain : "", (host->flags & HOST_ADDRESS_FLAG_IP) ? ((host->ip.family==IP_ADDRESS_FAMILY_IPv4) ? host->ip.ip.v4 : host->ip.ip.v6) : "");
    return;

    error:
    logoutput_warning("add_net_service: error adding network port %i for %s", port->nr, hostname);

}

/* get all the current services
*/

void get_net_services(struct timespec *since)
{
    struct list_element_s *slist=NULL;
    struct simple_lock_s rlock;
    struct timespec dummy;
    void *index=NULL;
    unsigned int hashvalue=0;
    struct discover_resource_s *resource=NULL;

    logoutput("get_net_services");

    init_simple_readlock(&network.locking, &rlock);

    if (since==NULL) {

	dummy.tv_sec=0;
	dummy.tv_nsec=0;
	since=&dummy;

    }

    processlist:

    if (simple_lock(&rlock)<0) return;

    getresource:

    while (hashvalue<DISCOVER_NETWORK_HASHSIZE && resource==NULL) {

	resource=(struct discover_resource_s *) get_next_hashed_resource(&index, hashvalue);
	if (resource) break;
	hashvalue++;

    }

    /* look only at those which have been changed since */

    if (resource==NULL) {

	logoutput_warning("get_net_services: no more resources found");

    } else if (resource->type != DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) {

	goto getresource;

    } else {

	if (resource->changed.tv_sec > since->tv_sec || (resource->changed.tv_sec == since->tv_sec && resource->changed.tv_nsec > since->tv_nsec)) {

	    /* service is "new" */

	    if (simple_upgradelock(&rlock)==0) {

		resource->flags |= DISCOVER_RESOURCE_FLAG_BUSY;
		simple_downgradelock(&rlock);

	    }

	    (* network.process_new_service)(resource, network.ptr);

	    if (simple_upgradelock(&rlock)==0) {

	        resource->flags &= ~DISCOVER_RESOURCE_FLAG_BUSY;
		simple_downgradelock(&rlock);

	    }

	}

	goto getresource;

    }

    simple_unlock(&rlock);

}

static void cb_discover_default(struct discover_resource_s *r, void *ptr)
{

    if (r->type==DISCOVER_RESOURCE_TYPE_NETWORK_GROUP) {

	logoutput_info("cb_discover_default: found network group %s", r->service.group.name);

    } else if (r->type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {

	logoutput_info("cb_discover_default: found network host %s", r->service.host.address.hostname);

    } else if (r->type==DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) {

	logoutput_info("cb_discover_default: found network socket %i", r->service.socket.port.nr);

    }

}

int init_discover_group(void (* cb)(struct discover_resource_s *r, void *ptr), void *ptr)
{

    if (cb==NULL) cb=cb_discover_default;

    /* init network: the root for all network resouces  */

    memset(&network, 0, sizeof(struct discover_network_s));

    network.ctr=1;
    init_list_header(&network.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_simple_locking(&network.locking, 0);
    network.changed.tv_sec=0;
    network.changed.tv_nsec=0;
    network.process_new_service=cb;
    network.ptr=ptr;
    for (unsigned int i=0; i<DISCOVER_NETWORK_HASHSIZE; i++) init_list_header(&network.hash[i], SIMPLE_LIST_TYPE_EMPTY, NULL);

    /* init the nodomain group: container for hosts for which no domain is found */

    memset(&nodomain, 0, sizeof(struct discover_resource_s));

    nodomain.unique=0;
    nodomain.type=DISCOVER_RESOURCE_TYPE_NETWORK_GROUP;
    nodomain.flags=0;
    init_list_header(&nodomain.service.group.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_element(&nodomain.list, NULL);
    get_current_time(&nodomain.found);
    memcpy(&nodomain.changed, &nodomain.found, sizeof(struct timespec));
    nodomain.flags=DISCOVER_RESOURCE_FLAG_NODOMAIN;

    add_list_element_first(&network.header, &nodomain.list);
    init_list_header(&queue, SIMPLE_LIST_TYPE_EMPTY, NULL);

    return 0;
}

int rlock_discover_network(struct simple_lock_s *lock)
{
    init_simple_readlock(&network.locking, lock);
    return simple_lock(lock);
}

int wlock_discover_network(struct simple_lock_s *lock)
{
    init_simple_writelock(&network.locking, lock);
    return simple_lock(lock);
}

int unlock_discover_network(struct simple_lock_s *lock)
{
    return simple_unlock(lock);
}

void free_discover_records()
{
    for (unsigned int i=0; i<DISCOVER_NETWORK_HASHSIZE; i++) {
	struct list_element_s *list=get_list_head(&network.hash[i], SIMPLE_LIST_FLAG_REMOVE);

	while (list) {
	    struct hash_element_s *h=(struct hash_element_s *)((char *) list - offsetof(struct hash_element_s, list));

	    if (h->data) {

		struct discover_resource_s *resource=(struct discover_resource_s *) h->data;
		free_discover_resource(resource);
		h->data=NULL;

	    }

	    free(h);
	    list=get_list_head(&network.hash[i], SIMPLE_LIST_FLAG_REMOVE);

	}

    }

}

struct discover_resource_s *get_next_discover_resource(struct discover_resource_s *parent, struct discover_resource_s *r, unsigned char type)
{
    struct list_element_s *l=NULL;

    if (r==NULL) {
	struct list_header_s *header=NULL;

	if (type==DISCOVER_RESOURCE_TYPE_NETWORK_GROUP) {

	    /* network is a different struct */

	    header=&network.header;

	} else {

	    if (parent->type==DISCOVER_RESOURCE_TYPE_NETWORK_GROUP) {

		header=&parent->service.group.header;

	    } else if (parent->type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {

		header=&parent->service.host.header;

	    } else if (parent->type==DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET) {

		header=&parent->service.socket.header;

	    }

	}

	if (header) l=get_list_head(header, 0);

    } else {

	l=get_next_element(&r->list);
	r=NULL;

    }

    if (l) r=(struct discover_resource_s *) ((char *) l - offsetof(struct discover_resource_s, list));

    return r;

}

struct timespec *get_discover_network_changed(unsigned char type, uint32_t unique)
{

    if (type==0) {

	return &network.changed;

    } else {
	struct discover_resource_s *resource=lookup_resource_id(unique);

	if (resource && resource->type==type) return &resource->changed;

    }

    return NULL;
}

void synchronize_discover_resources()
{
    struct list_element_s *list=NULL;

    pthread_mutex_lock(&queue_mutex);

    list=get_list_head(&queue, SIMPLE_LIST_FLAG_REMOVE);

    while(list) {

	struct discover_queue_s *data=(struct discover_queue_s *)((char *) list - offsetof(struct discover_queue_s, list));
	struct network_port_s port;

	port.nr=data->port;
	port.type=data->fam;

	add_net_service(data->method, data->name, &data->host, &port, data->service, NULL);

	free(data->name);
	free(data);
	list=get_list_head(&queue, SIMPLE_LIST_FLAG_REMOVE);

    }

    pthread_mutex_unlock(&queue_mutex);

}

void queue_service_avahi(const char *name, const char *hostname, char *ipv4, unsigned int port, const char *type)
{
    unsigned int service=0;
    unsigned int fam=0;

    if (strcmp(type, "_sftp-ssh._tcp")==0) { 

	service=(DISCOVER_RESOURCE_FLAG_SFTP | DISCOVER_RESOURCE_FLAG_SSH);
	fam=_NETWORK_PORT_TCP;

    } else if (strcmp(type, "_ssh._tcp")==0) {

	service=DISCOVER_RESOURCE_FLAG_SSH;
	fam=_NETWORK_PORT_TCP;

    } else if (strcmp(type, "_smb._tcp")==0) {

	service=DISCOVER_RESOURCE_FLAG_SMB;
	fam=_NETWORK_PORT_TCP;

    } else if (strcmp(type, "_nfs._tcp")==0) {

	service=DISCOVER_RESOURCE_FLAG_NFS;
	fam=_NETWORK_PORT_TCP;

    }

    if (service>0) {
	struct host_address_s host;
	struct network_port_s netport;

	init_host_address(&host);
	set_host_address(&host, name, ipv4, NULL);

	netport.nr=port;
	netport.type=fam;

	logoutput("queue_service_avahi: add name %s ipv4 %s port %i type %s", name, ipv4, port, type);

	add_net_service(DISCOVER_METHOD_AVAHI, hostname, &host, &netport, service, NULL);

    } else {

	logoutput("queue_service_avahi: ignore name %s ipv4 %s port %i type %s", name, ipv4, port, type);

    }

    /*struct discover_queue_s *data=malloc(sizeof(struct discover_queue_s));

    if (data) {

	memset(data, 0, sizeof(struct discover_queue_s));

	init_list_element(&data->list, NULL);
    	init_host_address(&data->host);
	set_host_address(&data->host, name, ipv4, NULL);

	data->name=strdup(hostname);
	data->method=DISCOVER_METHOD_AVAHI;
	data->port=port;
	data->fam=fam;
	data->service=service;

	pthread_mutex_lock(&queue_mutex);
	add_list_element_last(&queue, &data->list);
	pthread_mutex_unlock(&queue_mutex);

	logoutput("queue_service_avahi: ready");

    } else {

	logoutput_warning("queue_service_avahi: unable to allocate data for queue");

    } */

}

void add_net_service_avahi(const char *name, const char *hostname, char *ipv4, const char *domain, unsigned int port, const char *type)
{
    queue_service_avahi(name, hostname, ipv4, port, type);
}

void discover_services()
{

    if (fs_options.network.flags & _OPTIONS_NETWORK_DISCOVER_METHOD_FILE) {

	browse_services_staticfile(&fs_options.network.discover_static_file);

    }

    browse_services_avahi(0, add_net_service_avahi);

}

const char *get_name_resource_flag(struct discover_resource_s *r)
{

    if (r->flags & DISCOVER_RESOURCE_FLAG_SSH) {

	return "SSH";

    } else if (r->flags & DISCOVER_RESOURCE_FLAG_SFTP) {

	return "SFTP";

    } else if (r->flags & DISCOVER_RESOURCE_FLAG_SMB) {

	return "SMB";

    } else if (r->flags & DISCOVER_RESOURCE_FLAG_WEBDAV) {

	return "WEBDAV";

    } else if (r->flags & DISCOVER_RESOURCE_FLAG_NFS) {

	return "NFS";

    }

    return "UNKNOWN";

}

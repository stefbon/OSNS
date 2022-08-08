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

#include "osns-protocol.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-system.h"
#include "libosns-threads.h"

#include "avahi-detect.h"
#include "netcache.h"

struct netcache_watch_s {
    uint64_t				unique;
    uint32_t				offset;
    uint32_t				watchid;
    struct list_element_s		list;
    void				(* cb)( uint64_t unique, uint32_t offset, uint32_t watchid);
};

#define HASHTABLE_SIZE			64

static struct list_header_s hashtable_records[HASHTABLE_SIZE];
static uint32_t hashtable_offset=0;
static struct list_header_s records_queue;
static struct list_header_s watches;
static pthread_t threadid=0;

static void add_record_hashtable(struct network_record_s *record)
{
    unsigned int hashvalue=0;
    struct list_header_s *header=NULL;

    record->offset=hashtable_offset;
    hashvalue=(hashtable_offset % HASHTABLE_SIZE);
    header=&hashtable_records[hashvalue];

    write_lock_list_header(header);

    /* while waiting for the lock to be released if it has been locked,
	the offset may be increased,
	and the wrong header of the hashtable is selected */

    while (record->offset != hashtable_offset) {

	write_unlock_list_header(header);
	record->offset=hashtable_offset;
	hashvalue=(hashtable_offset % HASHTABLE_SIZE);
	header=&hashtable_records[hashvalue];
	write_lock_list_header(header);

    }

    add_list_element_last(header, &record->list); /* by adding as last the list "same hashvalue" is ordered from small to bigger offset */
    hashtable_offset++;
    write_unlock_list_header(header);

}

static unsigned char translate_dnssd_type(const char *type, struct network_record_s *record)
{
    unsigned char supported=0;

    if (strcmp(type, "_sftp-ssh._tcp")==0) { 

	/* sftp over ssh using tcp */

	record->service=NETWORK_SERVICE_TYPE_SFTP;
	record->transport=NETWORK_SERVICE_TYPE_SSH;
	record->port.type=_NETWORK_PORT_TCP;
	supported=1;

    } else if (strcmp(type, "_ssh._tcp")==0) {

	/* ssh using tcp */

	record->service=NETWORK_SERVICE_TYPE_SSH;
	record->port.type=_NETWORK_PORT_TCP;
	supported=1;

    } else if (strcmp(type, "_smb._tcp")==0) {

	/* smb using tcp */

	record->service=NETWORK_SERVICE_TYPE_SMB;
	record->port.type=_NETWORK_PORT_TCP;
	supported=1;

    } else if (strcmp(type, "_nfs._tcp")==0) {

	/* nfs using tcp */

	record->service=NETWORK_SERVICE_TYPE_NFS;
	record->port.type=_NETWORK_PORT_TCP;
	supported=1;

    }

    return supported;

}

static void process_records_queue(void *ptr)
{
    struct list_element_s *list=NULL;

    write_lock_list_header(&records_queue);

    if (threadid==0) {

	threadid=pthread_self();

    } else {

	write_unlock_list_header(&records_queue);
	return;

    }

    tryrecord:

    list=get_list_head(&records_queue, SIMPLE_LIST_FLAG_REMOVE);

    write_unlock_list_header(&records_queue);

    if (list) {
	struct network_record_s *record=(struct network_record_s *)((char *) list - offsetof(struct network_record_s, list));

	add_record_hashtable(record);

	/* test the pending watches */

	read_lock_list_header(&watches);
	list=get_list_head(&watches, 0);

	while (list) {
	    struct netcache_watch_s *watch=(struct netcache_watch_s *)((char *) list - offsetof(struct netcache_watch_s, list));

	    (* watch->cb)(watch->unique, record->offset, watch->watchid);
	    list=get_next_element(list);

	}

	read_unlock_list_header(&watches);
	goto tryrecord;

    }

    write_lock_list_header(&records_queue);
    threadid=0;
    write_unlock_list_header(&records_queue);

}

static void complete_missing_data(struct network_record_s *record)
{

    if (record->hostname==NULL) {
	char *ip=((record->ip.family==AF_INET) ? record->ip.addr.v4 : record->ip.addr.v6);
	char *hostname=NULL;

	hostname=lookupname_dns(ip);

	if (hostname) {

	    logoutput("complete_missing_data: got hostname %s for ip %s via dns", hostname, ip);

	} else {

	    hostname=get_hostname_addrinfo(&record->ip);

	}

	if (hostname) {
	    char *sep=strchr(hostname, '.');

	    logoutput("complete_missing_data: got hostname %s", hostname);

	    if (sep) {

		*sep='\0';
		record->hostname=strdup(hostname);
		if (record->domain==NULL) record->domain=strdup(sep+1);

	    } else {

		record->hostname=hostname;
		hostname=NULL;

	    }

	    if (hostname) free(hostname);

	}

    }

}

void add_detected_network_service(const char *hostname, const char *domain, char *ip, unsigned int ip_family, uint16_t port, unsigned int flags, const char *type)
{
    struct network_record_s *record=NULL;

    if (port==0 || ip_family==0 || ip==NULL) return;	/* ip/family and port are required, the rest is additional */

    /* TODO: prevent doubles */

    record=malloc(sizeof(struct network_record_s));

    if (record) {
	struct list_element_s *list=NULL;

	memset(record, 0, sizeof(struct network_record_s));

	record->flags=flags;
	get_current_time_system_time(&record->created);
	set_system_time(&record->removed, 0, 0);
	record->hostname=NULL;
	record->domain=NULL;

	if (flags & OSNS_NETCACHE_QUERY_FLAG_DNSSD) {

	    if (translate_dnssd_type(type, record)==0) logoutput_warning("add_detected_network_service: dnssd type %s not supported", type);

	} else {

	    if (hostname) record->hostname=strdup(hostname);
	    if (domain) record->domain=strdup(domain);

	}

	record->ip.family=ip_family;

	if (ip_family==IP_ADDRESS_FAMILY_IPv4) {

	    strcpy(record->ip.addr.v4, ip);

	} else if (ip_family==IP_ADDRESS_FAMILY_IPv6) {

	    strcpy(record->ip.addr.v6, ip);

	}

	record->port.nr=port;
	record->offset=0;
	init_list_element(&record->list, NULL);
	complete_missing_data(record);

	/* add to the queue to be processed */

	write_lock_list_header(&records_queue);
	add_list_element_last(&records_queue, &record->list);

	if (threadid==0) {

	    work_workerthread(NULL, 0, process_records_queue, NULL, NULL);

	}

	write_unlock_list_header(&records_queue);

	logoutput_debug("add_detected_network_service: queued %s at %s:%i", type, ip, port);

    } else {

	logoutput_warning("add_detected_network_service: not able to allocate %i bytes for network record", sizeof(struct network_record_s));

    }

}

void init_network_cache()
{
    /* initialize hashtable */

    for (unsigned int i=0; i<HASHTABLE_SIZE; i++) init_list_header(&hashtable_records[i], SIMPLE_LIST_TYPE_EMPTY, NULL);

    /* init ordered list of watches */
    init_list_header(&watches, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&records_queue, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

void start_network_cache()
{
    browse_services_avahi(0);
}

void stop_network_cache()
{
    stop_services_avahi();
}

void remove_cached_network_records()
{
    unsigned int hashvalue=0;
    struct list_element_s *list=NULL;

    while (hashvalue < HASHTABLE_SIZE) {

	list=get_list_head(&hashtable_records[hashvalue], SIMPLE_LIST_FLAG_REMOVE);

	while (list) {
	    struct network_record_s *record=(struct network_record_s *)((char *) list - offsetof(struct network_record_s, list));

	    if (record->hostname) free(record->hostname);
	    if (record->domain) free(record->domain);
	    free(record);
	    list=get_list_head(&hashtable_records[hashvalue], SIMPLE_LIST_FLAG_REMOVE);

	}

	hashvalue++;

    }

    list=get_list_head(&watches, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct netcache_watch_s *watch=(struct netcache_watch_s *)((char *) list - offsetof(struct netcache_watch_s, list));

	list=get_list_head(&watches, SIMPLE_LIST_FLAG_REMOVE);

    }

}

struct network_record_s *find_network_record(uint32_t offset, unsigned char flags)
{
    unsigned int hashvalue=0;
    struct list_header_s *header=NULL;
    struct list_element_s *list=NULL;
    struct network_record_s *record=NULL;

    trytablerow:

    hashvalue=(offset % HASHTABLE_SIZE);
    header=&hashtable_records[hashvalue];
    read_lock_list_header(header);

    logoutput_debug("find_network_record: hash %u", hashvalue);

    list=get_list_head(header, 0);
    while (list) {

	record=(struct network_record_s *)((char *) list - offsetof(struct network_record_s, list));
	if (record->offset==offset) break;
	list=get_next_element(list);
	record=NULL;

    }

    read_unlock_list_header(header);

    if (record==NULL && (flags & FIND_NETWORK_RECORD_FLAG_EXACT)==0) {

	/* if not found try the next offset */
	offset++;
	if (offset<hashtable_offset) goto trytablerow;

    }

    return record;

}

struct network_record_s *get_next_network_record(struct network_record_s *record)
{
    unsigned int hashvalue=0;
    struct list_element_s *list=NULL;

    if (record) {

	list=get_next_element(&record->list);
	if (list) goto found;
	hashvalue=(record->offset % HASHTABLE_SIZE) + 1;

    }

    while (hashvalue < HASHTABLE_SIZE) {
	struct list_header_s *h=&hashtable_records[hashvalue];

	list=get_list_head(h, 0);
	if (list) goto found;
	hashvalue++;

    }

    return NULL;

    found:

    return (struct network_record_s *)((char *) list - offsetof(struct network_record_s, list));
}

int add_netcache_watch(uint64_t unique, uint32_t watchid, void (* cb)(uint64_t unique, uint32_t offset, uint32_t watchid))
{
    struct netcache_watch_s *watch=NULL;
    int result=-1;

    watch=malloc(sizeof(struct netcache_watch_s));

    if (watch) {

	memset(watch, 0, sizeof(struct netcache_watch_s));
	init_list_element(&watch->list, NULL);
	watch->unique=unique;
	watch->cb=cb;

	write_lock_list_header(&watches);
	add_list_element_last(&watches, &watch->list);
	write_unlock_list_header(&watches);

	result=0;

    }

    return result;

}

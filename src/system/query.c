/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-connection.h"

#include "osns-protocol.h"

#include "receive.h"
#include "osns/reply.h"

#include "netcache.h"
#include "query.h"
#include "query-netcache.h"
#include "query-mountinfo.h"

static struct list_header_s openquery_requests;
static pthread_mutex_t openquery_mutex=PTHREAD_MUTEX_INITIALIZER;

#define OPENQUERY_REQUEST_OPEN				1
#define OPENQUERY_REQUEST_READ				2
#define READQUERY_MAXSIZE				8192

static unsigned int cb_read_default(struct openquery_request_s *request, struct readquery_request_s *readquery)
{
    return 0;
}
static void cb_close_default(struct openquery_request_s *request)
{
}
static int cb_filter_default(struct openquery_request_s *request, void *ptr)
{
    return 1; /* default accept everything */
}

static struct openquery_request_s *create_openquery_request(struct osns_receive_s *r)
{
    struct openquery_request_s *request=malloc(sizeof(struct openquery_request_s));

    if (request) {

	memset(request, 0, sizeof(struct openquery_request_s));

	request->sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
	get_current_time_system_time(&request->created);
	request->id=0;
	init_list_element(&request->list, NULL);
	request->offset=0;
	request->status=0;
	request->cb_read=cb_read_default;
	request->cb_close=cb_close_default;
	request->cb_filter=cb_filter_default;
	request->flags=0;
	request->valid=0;

    }

    return request;

}

static struct openquery_request_s *lookup_openquery_request(struct osns_receive_s *r, struct name_string_s *handle, unsigned char opcode)
{
    struct openquery_request_s *request=NULL;
    uint64_t unique=0;
    int64_t seconds=0;
    int32_t nseconds=0;
    uint32_t id=0;

    if (handle->len >= 24) {
	char *data=handle->ptr;
	unsigned int pos=0;
	struct list_element_s *list=NULL;

	unique=get_uint64(&data[pos]);
	pos+=8;
	seconds=get_int64(&data[pos]);
	pos+=8;
	nseconds=get_uint32(&data[pos]);
	pos+=4;
	id=get_uint32(&data[pos]);
	pos+=4;

	pthread_mutex_lock(&openquery_mutex);
	list=get_list_head(&openquery_requests, 0);

	while (list) {
	    struct osns_systemconnection_s *sc=NULL;

	    request=(struct openquery_request_s *)((char *) list - offsetof(struct openquery_request_s, list));
	    sc=request->sc;

	    if (unique==sc->connection.ops.client.unique && get_system_time_sec(&request->created)==seconds && get_system_time_nsec(&request->created)==nseconds && request->id==id) {

		if (opcode==OSNS_MSG_READQUERY) {

		    if (request->status & OPENQUERY_REQUEST_OPEN) {

			request->status += OPENQUERY_REQUEST_READ;

		    } else {

			request=NULL;

		    }

		} else if (opcode==OSNS_MSG_CLOSEQUERY) {

		    if (request->status & OPENQUERY_REQUEST_OPEN) request->status &= ~OPENQUERY_REQUEST_OPEN;

		}

		break;

	    }

	    list=get_next_element(list);
	    request=NULL;

	}

	pthread_mutex_unlock(&openquery_mutex);

    }

    return request;

}

static void release_openquery_request(struct openquery_request_s *request, unsigned char opcode)
{

    pthread_mutex_lock(&openquery_mutex);

    if (opcode==OSNS_MSG_READQUERY) {

	if (request->status >= OPENQUERY_REQUEST_READ) request->status -= OPENQUERY_REQUEST_READ;

    } else if (opcode==OSNS_MSG_CLOSEQUERY) {

	if (request->status & OPENQUERY_REQUEST_OPEN) request->status &= ~OPENQUERY_REQUEST_OPEN;

    }

    if (request->status==0) {

	(* request->cb_close)(request);
	remove_list_element(&request->list);
	free(request);

    }

    pthread_mutex_unlock(&openquery_mutex);
}

/*
    reply with a handle */

static int system_reply_request_handle(struct osns_receive_s *r, uint32_t id, struct openquery_request_s *request)
{
    struct osns_systemconnection_s *sc=request->sc;
    char tmp[24];
    unsigned int pos=0;
    struct name_string_s handle=NAME_STRING_SET(24, tmp);

    /* TODO:
	encrypt the following 24 bytes ?? */

    store_uint64(&tmp[pos], sc->connection.ops.client.unique);
    pos+=8;

    store_uint64(&tmp[pos], (uint64_t) get_system_time_sec(&request->created));
    pos+=8;

    store_uint32(&tmp[pos], (uint32_t) get_system_time_nsec(&request->created));
    pos+=4;

    store_uint32(&tmp[pos], request->id);
    pos+=4;

    return osns_reply_name(r, id, &handle);

}

/*
    receive a request to open a batch of cached services

    name string				what
    uint32				flags
    ATTR				attr

    a handle is sent as answer
    unique || sec || nsec || id = (8bytes + 8bytes + 4bytes + 4bytes = 24 bytes)

*/

void process_msg_openquery(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len)
{
    unsigned int status=OSNS_STATUS_PROTOCOLERROR;
    struct name_string_s what=NAME_STRING_INIT;
    unsigned int pos=0;
    unsigned char tmp=0;

    if (len<=9) goto errorout;
    pos=read_name_string(data, len, &what);
    if (pos<=1 || pos>len) goto errorout;

    if (compare_name_string(&what, 'c', "netcache")==0) {

	tmp=OSNS_LIST_TYPE_NETCACHE;

    } else if (compare_name_string(&what, 'c', "mountinfo")==0) {

	tmp=OSNS_LIST_TYPE_MOUNTINFO;

    }

    status=OSNS_STATUS_NOTSUPPORTED;

    if (tmp>0) {
	struct openquery_request_s *request=NULL;

	memmove(data, (char *)(data + pos), (len-pos));
	len-=pos;

	status=OSNS_STATUS_SYSTEMERROR;
	request=create_openquery_request(r);
	if (request==NULL) goto errorout;

	if (tmp==OSNS_LIST_TYPE_NETCACHE) {

	    status=process_openquery_netcache(r, id, data, len, request);

	} else if (tmp==OSNS_LIST_TYPE_MOUNTINFO) {

	    status=process_openquery_mountinfo(r, id, data, len, request);

	}

	if (status==0) {

	    if (system_reply_request_handle(r, id, request)>0) {

		pthread_mutex_lock(&openquery_mutex);
		add_list_element_first(&openquery_requests, &request->list);
		request->status=OPENQUERY_REQUEST_OPEN;
		pthread_mutex_unlock(&openquery_mutex);
		return;

	    } else {

		/* reply not send: request is not going to be used, best to free it */
		(* request->cb_close)(request);
		free(request);
		status=OSNS_STATUS_SYSTEMERROR;

	    }

	}

    }

    errorout:
    osns_reply_status(r, id, status, NULL, 0);

}

/*
    receive a request to get a batch cached records

    name string				handle
    uint32				size
    uint32				offset;
*/

void process_msg_readquery(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len)
{
    struct name_string_s handle=NAME_STRING_INIT;
    unsigned int pos=0;
    unsigned int status=OSNS_STATUS_PROTOCOLERROR;
    unsigned int size=0;
    unsigned int offset=0;
    struct openquery_request_s *request=NULL;

    // logoutput_debug("process_msg_readquery: id %u len %u", id, len);

    if (len<=9) goto errorout;
    pos=read_name_string(data, len, &handle);
    if ((pos <= 1) || (pos + 8 > len)) {

	logoutput_debug("process_msg_readquery: handle invalid (pos=%u)", pos);
	goto errorout;

    }

    size=get_uint32(&data[pos]);
    pos+=4;
    offset=get_uint32(&data[pos]);
    pos+=4;

    /* 20220130 TODO:
    - here some sanity check for size ... */

    status=OSNS_STATUS_PROTOCOLERROR;
    if (size==0 || size > READQUERY_MAXSIZE) {

	logoutput_debug("process_msg_readquery: size invalid (size=%u)", size);
	goto errorout;

    }

    status=OSNS_STATUS_HANDLENOTFOUND;
    request=lookup_openquery_request(r, &handle, OSNS_MSG_READQUERY);

    if (request==NULL) {

	logoutput_debug("process_msg_readquery: handle not found");
	goto errorout;

    } else {
	char buffer[size];
	unsigned int count=0;
	unsigned int nbytes=0;
	struct readquery_request_s readquery;

	readquery.buffer=buffer;
	readquery.size=size;
	readquery.offset=offset;
	readquery.count=0;

	nbytes=(* request->cb_read)(request, &readquery);
	if (osns_reply_records(r, id, readquery.count, buffer, nbytes)==-1) logoutput_warning("process_msg_readquery: error sending reply");

    }

    release_openquery_request(request, OSNS_MSG_READQUERY);
    return;

    errorout:
    osns_reply_status(r, id, status, NULL, 0);
}

/*  receive a request to close a batch of cached services
    name string				handle
*/

void process_msg_closequery(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len)
{
    struct name_string_s handle=NAME_STRING_INIT;
    unsigned int pos=0;
    unsigned int status=OSNS_STATUS_PROTOCOLERROR;
    struct openquery_request_s *request=NULL;

    if (len < 2) goto errorout;
    pos=read_name_string(data, len, &handle);
    if ((pos<=1) || (pos + 8 > len)) goto errorout;

    status=OSNS_STATUS_HANDLENOTFOUND;
    request=lookup_openquery_request(r, &handle, OSNS_MSG_CLOSEQUERY);
    if (request==NULL) goto errorout;
    osns_reply_status(r, id, 0, NULL, 0);
    release_openquery_request(request, OSNS_MSG_CLOSEQUERY);
    return;

    errorout:
    osns_reply_status(r, id, status, NULL, 0);
}

void init_system_query()
{
    init_list_header(&openquery_requests, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

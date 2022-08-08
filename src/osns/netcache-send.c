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
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-network.h"

#include "osns-protocol.h"
#include "osns_client.h"

#include "osns/send.h"
#include "netcache-read.h"
#include "hashtable.h"
#include "record.h"

static unsigned char get_osns_protocol_handlesize(struct osns_connection_s *client)
{
    unsigned char size=0;

    if (client->protocol.version==1) {

	size=client->protocol.level.one.handlesize;

    }

    return size;
}

/* osns openquery cb */

struct _openquery_data_s {
    struct name_string_s 	*handle;
    unsigned int		status;
};

static void cb_openquery_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct _openquery_data_s *openquery=(struct _openquery_data_s *) p->ptr;
    struct osns_receive_s *r=p->r;

    logoutput_debug("cb_openquery_response: id %u type=%u size=%u", p->id, type, size);

    p->reply=type;

    if (type==OSNS_MSG_NAME) {

	if (size>1) {
	    unsigned char len=(unsigned char) data[0];
	    struct name_string_s *handle=openquery->handle;

	    logoutput_debug("cb_openquery_response: handle len %u", len);

	    if ((size >= 1 + len) && (len<=handle->len)) {

		memcpy(handle->ptr, &data[1], len);
		handle->len=len;
		logoutput_debug("cb_openquery_response: signal finish");
		signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
		return;

	    }

	}

    } else if (type==OSNS_MSG_STATUS) {

	if (size>=4) {

	    openquery->status=get_uint32(data);
	    logoutput_debug("cb_openquery_response: status %u", openquery->status);
	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    }

    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_ERROR);

}

/* osns readquery cb */

struct _readquery_data_s {
    char			*command;
    void			(* cb)(struct query_netcache_attr_s *attr, unsigned int flags);
    unsigned int		offset;
    unsigned int		status;
    unsigned int		flags;
};

static int cb_readquery_record(struct osns_record_s *rec, unsigned int count, unsigned int index, void *ptr)
{
    struct _readquery_data_s *readquery=(struct _readquery_data_s *) ptr;
    int result=-1;
    char *data=rec->data;
    unsigned int size=rec->len;

    if (strcmp(readquery->command, "netcache")==0) {

	/* netcache:
	    every record looks like:
	    - uint32			offset
	    - uint32			valid
	    - NETCACHE ATTR			attr */

	unsigned int offset=0;
	unsigned int valid=0;
	unsigned int pos=0;

	offset=get_uint32(&data[pos]);
	pos+=4;
	valid=get_uint32(&data[pos]);
	pos+=4;

	logoutput_debug("cb_readquery_record: offset=%u valid=%u", offset, valid);

	if (valid>0) {
	    struct query_netcache_attr_s attr;
	    int tmp=0;

	    memset(&attr, 0, sizeof(struct query_netcache_attr_s));
	    if (read_record_netcache(&data[pos], (size - pos), valid, &attr)==-1) goto out;
	    (* readquery->cb)(&attr, readquery->flags);

	}

	if (offset > readquery->offset) readquery->offset=offset;
	result=(int) pos;

    } else {

	logoutput_debug("cb_readquery_record: command %s not supported", readquery->command);

    }

    out:
    return result;

}

static void cb_readquery_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct _readquery_data_s *readquery=(struct _readquery_data_s *) p->ptr;
    struct osns_receive_s *r=p->r;

    logoutput_debug("cb_readquery_response: type=%u size=%u", type, size);

    p->reply=type;

    if (type==OSNS_MSG_RECORDS) {
	int result=0;

	result=process_osns_records(data, size, cb_readquery_record, p->ptr);
	if (result==-1) goto errorout;
	signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	return;

    } else if (type==OSNS_MSG_STATUS) {

	if (size>=4) {

	    readquery->status=get_uint32(data);
	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    }

    errorout:
    logoutput_debug("cb_readquery_response: error parsing record");
    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_ERROR);

}

/* osns closequery cb */

struct _closequery_data_s {
    unsigned int		status;
};

static void cb_closequery_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct _closequery_data_s *closequery=(struct _closequery_data_s *) p->ptr;
    struct osns_receive_s *r=p->r;

    logoutput_debug("cb_closequery_response");

    p->reply=type;

    if (type==OSNS_MSG_STATUS) {

	if (size>=4) {

	    closequery->status=get_uint32(data);
	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    }

    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_ERROR);

}

void osns_system_query_netcache(struct osns_connection_s *oc, struct osns_record_s *filter, void (* cb)(struct query_netcache_attr_s *attr, unsigned int flags))
{
    struct osns_receive_s *r=&oc->receive;
    struct osns_packet_s packet;
    struct name_string_s cmd=NAME_STRING_SET(0, "netcache");
    char buffer[128];
    unsigned int offset=0;
    struct name_string_s handle=NAME_STRING_SET(128, buffer);
    unsigned int valid=(OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME | OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN | OSNS_NETCACHE_QUERY_ATTR_IPV4 | OSNS_NETCACHE_QUERY_ATTR_IPV6 | OSNS_NETCACHE_QUERY_ATTR_PORT | OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY | OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE | OSNS_NETCACHE_QUERY_ATTR_SERVICE | OSNS_NETCACHE_QUERY_ATTR_TRANSPORT);
    unsigned int flags=OSNS_NETCACHE_QUERY_FLAG_DNS;

    logoutput_debug("osns_system_query_netcache");

    init_osns_packet(&packet);
    memset(buffer, 0, 128);
    packet.cb=cb_openquery_response;
    packet.id=get_osns_msg_id(r);
    packet.r=r;

    logoutput_debug("osns_system_query_netcache: send openquery (id=%u)", packet.id);

    if (send_osns_msg_openquery(r, packet.id, &cmd, flags, valid, filter)>0) {
	struct _openquery_data_s openquery;

	openquery.handle=&handle;
	openquery.status=0;
	packet.ptr=(void *) &openquery;
	packet.reply=0;

	wait_osns_packet(r, &packet);

	if (packet.status & OSNS_PACKET_STATUS_FINISH) {

	    if (packet.reply == OSNS_MSG_NAME) {

		logoutput_debug("osns_system_query_netcache: received a handle (size=%u)", handle.len);

	    } else if (packet.reply==OSNS_MSG_STATUS) {

		logoutput_debug("osns_system_query_netcache: received a status %i", openquery.status);

	    } else {

		logoutput_debug("osns_system_query_netcache: received a %i message", packet.reply);
		goto out;

	    }

	} else {

	    if (packet.status & OSNS_PACKET_STATUS_TIMEDOUT) {

		logoutput_debug("osns_system_query_netcache: timedout");

	    } else if (packet.status & OSNS_PACKET_STATUS_ERROR) {

		logoutput_debug("osns_system_query_netcache: error");

	    } else {

		logoutput_debug("osns_system_query_netcache: failed, unknown reason");

	    }

	    goto out;

	}

    } else {

	logoutput("osns_system_query_netcache: unable to send open query");
	goto out;

    }

    packet.cb=cb_readquery_response;
    packet.status=0;

    startreadquery:

    packet.id=get_osns_msg_id(r);

    logoutput_debug("osns_system_query_netcache: send readquery (id=%u)", packet.id);

    if (send_osns_msg_readquery(r, packet.id, &handle, 4096, offset)>0) {
	struct _readquery_data_s readquery;

	readquery.command="netcache";
	readquery.offset=offset;
	readquery.status=0;
	readquery.cb=cb;
	readquery.flags=flags;
	packet.ptr=(void *) &readquery;
	packet.reply=0;

	wait_osns_packet(r, &packet);

	if (packet.status & OSNS_PACKET_STATUS_FINISH) {

	    if (packet.reply==OSNS_MSG_RECORDS) {

		if (readquery.offset>0) {

		    offset=readquery.offset + 1;
		    goto startreadquery;

		}

	    } else if (packet.reply==OSNS_MSG_STATUS) {

		logoutput("osns_system_query_netcache: received a status %i", readquery.status);

	    } else {

		logoutput("osns_system_query_netcache: received a %i message", packet.reply);

	    }

	} else if (packet.status & OSNS_PACKET_STATUS_TIMEDOUT) {

	    logoutput("osns_system_query_netcache: timedout");

	} else if (packet.status & OSNS_PACKET_STATUS_ERROR) {

	    logoutput("osns_system_query_netcache: error");

	} else {

	    logoutput("osns_system_query_netcache: failed, unknown reason");

	}

    } else {

	logoutput("osns_system_query_netcache: unable to send read query");

    }

    packet.cb=cb_closequery_response;
    packet.id=get_osns_msg_id(r);
    packet.status=0;

    logoutput_debug("osns_system_query_netcache: send closequery");

    if (send_osns_msg_closequery(r, packet.id, &handle)>0) {
	struct _closequery_data_s closequery;

	closequery.status=0;
	packet.ptr=(void *) &closequery;
	packet.reply=0;

	wait_osns_packet(r, &packet);

    } else {

	logoutput("osns_system_query_netcache: unable to send close query");
	goto out;

    }

    out:
    logoutput("osns_system_query_netcache: finish");

}

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
#include "osnsctl.h"
#include "osns/send.h"

#include "options.h"
#include "receive.h"
#include "read-netcache.h"
#include "print-netcache.h"
#include "hashtable.h"

static unsigned char get_osns_protocol_handlesize(struct osns_connection_s *client)
{
    unsigned char size=0;

    if (client->protocol.version==1) {

	size=client->protocol.level.one.handlesize;

    }

    return size;
}

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

struct _readquery_data_s {
    char			*command;
    unsigned int		offset;
    unsigned int		status;
};

static void cb_readquery_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct _readquery_data_s *readquery=(struct _readquery_data_s *) p->ptr;
    struct osns_receive_s *r=p->r;

    logoutput_debug("cb_readquery_response: type=%u size=%u", type, size);

    p->reply=type;

    if (type==OSNS_MSG_RECORDS) {
	unsigned int pos=0;
	unsigned int count=0;

	if (size<4) goto errorout;
	// logoutput_base64encoded(NULL, data, size, 1);

	count=get_uint32(&data[pos]);
	pos+=4;
	if (count==0) readquery->offset=0;

	logoutput_debug("cb_readquery_response: count=%u", count);

	for (unsigned int i=0; i<count; i++) {
	    unsigned int tmp_offset=0;
	    unsigned int valid=0;

	    if ((pos + 8) > size) goto errorout;

	    tmp_offset=get_uint32(&data[pos]);
	    pos+=4;
	    valid=get_uint32(&data[pos]);
	    pos+=4;

	    logoutput_debug("cb_readquery_response: offset=%u valid=%u", tmp_offset, valid);

	    if (valid>0) {

		if (strcmp(readquery->command, "netcache")==0) {
		    struct query_netcache_attr_s attr;
		    int tmp=0;

		    memset(&attr, 0, sizeof(struct query_netcache_attr_s));
		    tmp=read_record_netcache(&data[pos], (size - pos), valid, &attr);
		    if (tmp==-1) goto errorout;
		    print_record_netcache(&attr);
		    pos+=tmp;

		} else {

		    goto errorout;

		}

	    }

	    if (tmp_offset > readquery->offset) readquery->offset=tmp_offset;

	}

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

static char *strstr_filter(char *data, unsigned int size, char *token)
{
    char *pos=memchr(data, token[0], size);

    if (pos) {
	unsigned int left=(unsigned int)(data + size - pos);
	unsigned int tmp=strlen(token);

	if (left>=tmp && strncmp(pos, token, tmp)==0) return pos;

    }

    return NULL;

}

static unsigned int parse_netcache_filter_data(char *start, unsigned int len, struct query_netcache_attr_s *attr)
{
    unsigned int result=0;

    logoutput_debug("parse_netcache_filter_data: start %.*s", len, start);

    if (len >1 && start[0]=='(' && start[len-1]==')') {

	char *sep=memchr(&start[1], '=', len-2);

	if (sep) {
	    char *name=&start[1];
	    unsigned int tmp=(unsigned int)(sep - name + 1);

	    if (tmp==8 && memcmp(name, "service=", tmp)==0) {

		if (len > tmp + 2) {
		    char *value=sep+1;

		    tmp=(len - tmp - 2);
		    result=OSNS_NETCACHE_QUERY_ATTR_SERVICE;

		    if (tmp==3 && memcmp(value, "ssh", tmp)==0) {

			attr->service=OSNS_NETCACHE_SERVICE_TYPE_SSH;

		    } else if (tmp==3 && memcmp(value, "nfs", tmp)==0) {

			attr->service=OSNS_NETCACHE_SERVICE_TYPE_NFS;

		    } else if (tmp==4 && memcmp(value, "sftp", tmp)==0) {

			attr->service=OSNS_NETCACHE_SERVICE_TYPE_SFTP;

		    } else if (tmp==3 && memcmp(value, "smb", tmp)==0) {

			attr->service=OSNS_NETCACHE_SERVICE_TYPE_SMB;

		    } else if (tmp==6 && memcmp(value, "webdav", tmp)==0) {

			attr->service=OSNS_NETCACHE_SERVICE_TYPE_WEBDAV;

		    } else {

			result=0;

		    }

		}

	    } else if (tmp==10 && memcmp(name, "transport=", tmp)==0) {

		if (len > tmp + 2) {
		    char *value=sep+1;

		    tmp=(len - tmp - 2);
		    result=OSNS_NETCACHE_QUERY_ATTR_TRANSPORT;

		    if (tmp==3 && memcmp(value, "ssh", tmp)==0) {

			attr->transport=OSNS_NETCACHE_TRANSPORT_TYPE_SSH;

		    } else {

			result=0;

		    }

		}

	    } else if (tmp==7 && memcmp(name, "domain=", tmp)==0) {

		if (len > tmp + 2) {
		    struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN];

		    result=OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN;
		    name->ptr=sep+1;
		    name->len=(len - tmp - 2);

		}

	    }

	}

    }

    return result;

}

/* get the filters out of the filter argument from commandline

    this has the form:
    "(service=ssh)||(transport==ssh)"

    */

static int get_netcache_attr_filter(struct ctl_arguments_s *arg, struct query_netcache_attr_s *attr)
{
    struct osns_record_s *filter=&arg->cmd.list.filter;
    int result=-1;
    char *sep=NULL;
    unsigned int flags=0;

    if (filter->data==NULL || filter->len==0) return 0;

    /* look for the && and || tokens
	note: one level deep, no nested || and && constructions */

    sep=strstr_filter(filter->data, filter->len, "&&");
    if (sep==NULL) {

	sep=strstr_filter(filter->data, filter->len, "||");
	if (sep) flags |= OSNS_NETCACHE_QUERY_FLAG_VALID_OR;

    }

    if (sep) {
	unsigned int tmp=0;
	unsigned int valid=0;

	/* a && token found ... the part before and the part after have to be closed with brackets */

	/* first part */

	tmp=(unsigned int)(sep - filter->data);
	valid=parse_netcache_filter_data(filter->data, tmp, attr);
	if (valid==0) goto out;
	attr->valid |= valid;

	/* second part */

	tmp=(unsigned int)(filter->len - tmp - 2); /* total length minus the first part and length of token (=2) */
	valid=parse_netcache_filter_data((sep + 2), tmp, attr);
	if (valid==0) goto out;
	attr->valid |= valid;

	result=0;
	attr->flags |= flags;

    } else {
	unsigned int valid=0;

	/* no || and && token found: a simple filter with one parameter */

	valid=parse_netcache_filter_data(filter->data, filter->len, attr);
	if (valid==0) goto out;
	attr->valid |= valid;

	result=0;

    }

    out:
    logoutput_debug("get_netcache_attr_filter: result %i", result);
    return result;

}

static unsigned int write_netcache_filter_string(struct ctl_arguments_s *arg, struct query_netcache_attr_s *attr, struct attr_buffer_s *ab)
{

    if (attr==NULL) return 0;
    if (ab->buffer==NULL) {

	/* first time here ... initialize first */
	memset(attr, 0, sizeof(struct query_netcache_attr_s));
	if (get_netcache_attr_filter(arg, attr)==-1) return 0;

    }

    if (attr->valid>0) {

	(* ab->ops->rw.write.write_uint32)(ab, attr->flags);
	(* ab->ops->rw.write.write_uint32)(ab, attr->valid);

	if (attr->valid & OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN) {
	    struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN];

	    (* ab->ops->rw.write.write_uchar)(ab, name->len);
	    (* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) name->ptr, name->len);

	}

	if (attr->valid & OSNS_NETCACHE_QUERY_ATTR_SERVICE) (* ab->ops->rw.write.write_uint16)(ab, attr->service);
	if (attr->valid & OSNS_NETCACHE_QUERY_ATTR_TRANSPORT) (* ab->ops->rw.write.write_uint16)(ab, attr->transport);

    }

    return ab->pos;
}

void process_netcache_query(struct osns_connection_s *oc, struct ctl_arguments_s *arg)
{
    struct osns_receive_s *r=&oc->receive;
    struct osns_packet_s packet;
    struct name_string_s cmd=NAME_STRING_SET(0, "netcache");
    char buffer[128];
    unsigned int offset=0;
    struct name_string_s handle=NAME_STRING_SET(128, buffer);
    unsigned int valid=(OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME | OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN | OSNS_NETCACHE_QUERY_ATTR_IPV4 | OSNS_NETCACHE_QUERY_ATTR_IPV6 | OSNS_NETCACHE_QUERY_ATTR_PORT | OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY | OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE | OSNS_NETCACHE_QUERY_ATTR_SERVICE | OSNS_NETCACHE_QUERY_ATTR_TRANSPORT);
    unsigned int flags=OSNS_NETCACHE_QUERY_FLAG_DNS;
    struct query_netcache_attr_s filter;
    struct attr_buffer_s ab=INIT_ATTR_BUFFER_NOWRITE;
    unsigned int size=write_netcache_filter_string(arg, &filter, &ab);
    char tmp[size];
    struct osns_record_s filter_record;

    logoutput_debug("process_netcache_query");

    set_attr_buffer_write(&ab, tmp, size);
    size=write_netcache_filter_string(arg, &filter, &ab);

    if (size==0) {

	if (arg->cmd.list.filter.len>0) logoutput_debug("process_netcache_query: error processing filter %.*s", arg->cmd.list.filter.len, arg->cmd.list.filter.data);
	filter_record.len=0;
	filter_record.data=NULL;

    } else {

	filter_record.len=size;
	filter_record.data=tmp;

    }

    init_osns_packet(&packet);
    memset(buffer, 0, 128);

    packet.cb=cb_openquery_response;
    packet.id=get_osns_msg_id(r);
    packet.r=r;

    logoutput_debug("process_netcache_query: send openquery (id=%u)", packet.id);

    if (send_osns_msg_openquery(r, packet.id, &cmd, flags, valid, &filter_record)>0) {
	struct _openquery_data_s openquery;

	openquery.handle=&handle;
	openquery.status=0;
	packet.ptr=(void *) &openquery;
	packet.reply=0;

	wait_osns_packet(r, &packet);

	if (packet.status & OSNS_PACKET_STATUS_FINISH) {

	    if (packet.reply == OSNS_MSG_NAME) {

		logoutput_debug("process_netcache_query: received a handle (size=%u)", handle.len);

	    } else if (packet.reply==OSNS_MSG_STATUS) {

		logoutput_debug("process_netcache_query: received a status %i", openquery.status);

	    } else {

		logoutput_debug("process_netcache_query: received a %i message", packet.reply);
		goto out;

	    }

	} else if (packet.status & OSNS_PACKET_STATUS_TIMEDOUT) {

	    logoutput_debug("process_netcache_query: timedout");

	} else if (packet.status & OSNS_PACKET_STATUS_ERROR) {

	    logoutput_debug("process_netcache_query: error");

	} else {

	    logoutput_debug("process_netcache_query: failed, unknown reason");

	}

    } else {

	logoutput("process_netcache_query: unable to send open query");
	goto out;

    }

    packet.cb=cb_readquery_response;
    packet.status=0;

    startreadquery:

    packet.id=get_osns_msg_id(r);

    logoutput_debug("process_netcache_query: send readquery (id=%u)", packet.id);

    if (send_osns_msg_readquery(r, packet.id, &handle, 4096, offset)>0) {
	struct _readquery_data_s readquery;

	readquery.command="netcache";
	readquery.offset=offset;
	readquery.status=0;
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

		logoutput("process_netcache_query: received a status %i", readquery.status);

	    } else {

		logoutput("process_netcache_query: received a %i message", packet.reply);

	    }

	} else if (packet.status & OSNS_PACKET_STATUS_TIMEDOUT) {

	    logoutput("process_netcache_query: timedout");

	} else if (packet.status & OSNS_PACKET_STATUS_ERROR) {

	    logoutput("process_netcache_query: error");

	} else {

	    logoutput("process_netcache_query: failed, unknown reason");

	}

    } else {

	logoutput("process_netcache_query: unable to send read query");

    }

    packet.cb=cb_closequery_response;
    packet.id=get_osns_msg_id(r);
    packet.status=0;

    logoutput_debug("process_netcache_query: send closequery");

    if (send_osns_msg_closequery(r, packet.id, &handle)>0) {
	struct _closequery_data_s closequery;

	closequery.status=0;
	packet.ptr=(void *) &closequery;
	packet.reply=0;

	wait_osns_packet(r, &packet);

    } else {

	logoutput("process_netcache_query: unable to send close query");
	goto out;

    }

    out:
    logoutput("process_netcache_query: finish");

}

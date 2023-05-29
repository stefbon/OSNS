/*
  2010, 2011, 2012, 2103, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-send.h"
#include "ssh-extensions.h"

#include "ssh-utils.h"
#include "ssh-keyexchange.h"

int send_disconnect_message(struct ssh_connection_s *connection, unsigned int reason)
{
    const char *description=get_disconnect_reason(reason);
    unsigned int size=(description) ? strlen(description) : 0;
    char buffer[sizeof(struct ssh_payload_s) + 13 + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 13 + size);
    payload->type=SSH_MSG_DISCONNECT;

    *pos=SSH_MSG_DISCONNECT;
    pos++;

    store_uint32(pos, reason);
    pos+=4;

    store_uint32(pos, size);
    pos+=4;

    if (size>0) {

	memcpy(pos, description, size);
	pos+=size;

    }

    store_uint32(pos, 0); /* no language tag */
    pos+=4;
    payload->len=(unsigned int)(pos - payload->buffer);

    return write_ssh_packet(connection, payload);

}

int send_ignore_message(struct ssh_connection_s *connection, struct ssh_string_s *data)
{
    unsigned int size=(data) ? data->len : 0;
    char buffer[sizeof(struct ssh_payload_s) + 1 + 4 + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 5 + size);
    payload->type=SSH_MSG_IGNORE;

    *pos=SSH_MSG_IGNORE;
    pos++;

    store_uint32(pos, size);
    pos+=4;

    if (size>0) {

	memcpy(pos, data->ptr, size);
	pos+=size;

    }

    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(connection, payload);

}

int send_debug_message(struct ssh_connection_s *connection, struct ssh_string_s *debug)
{
    unsigned int size=(debug) ? debug->len : 0;
    char buffer[sizeof(struct ssh_payload_s) + 1 + 1 + 4 + size + 4];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 10 + size);
    payload->type=SSH_MSG_DEBUG;

    *pos=SSH_MSG_DEBUG;
    pos++;

    *pos=1; /* always display */
    pos++;

    store_uint32(pos, size);
    pos+=4;

    if (size>0) {

	memcpy(pos, debug->ptr, size);
	pos+=size;

    }

    store_uint32(pos, 0);
    pos+=4;

    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(connection, payload);

}

int send_unimplemented_message(struct ssh_connection_s *connection, unsigned int number)
{
    char buffer[sizeof(struct ssh_payload_s) + 5];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 5);
    payload->type=SSH_MSG_UNIMPLEMENTED;

    *pos=SSH_MSG_UNIMPLEMENTED;
    pos++;

    store_uint32(pos, number);
    pos+=4;
    payload->len=(unsigned int)(pos - payload->buffer);

    return write_ssh_packet(connection, payload);

}

struct kexinit_helper_s {
    struct algo_list_s 		*algos;
    unsigned int		type;
};

/* helper function to create a comma seperated list from the algo list for a specific type algo */

static void build_kexinit_commalist(struct msg_buffer_s *mb, void *ptr)
{
    struct kexinit_helper_s *helper=(struct kexinit_helper_s *) ptr;
    struct algo_list_s *algos=helper->algos;
    unsigned int type=helper->type;
    unsigned int ctr=0;
    unsigned char first=1;

    if (mb->data) {

	/*  take order in account: walk from the highest order to zero
	    this algorithm is not optional (bi-sect ordering is better) but since just about maximal 50
	    algo's in total are used this is not a bottleneck */

	for (unsigned int order=SSH_ALGO_ORDER_HIGH; order>0; order--) {

	    ctr=0;

	    while (algos[ctr].type>=0) {

		if (algos[ctr].type==type && algos[ctr].order==order) {
		    unsigned int len=strlen(algos[ctr].sshname);

#ifdef FS_WORKSPACE_DEBUG

		    logoutput("build_kexinit_commalist: found %s", algos[ctr].sshname);

#endif

		    if (first==0) {

			mb->data[mb->pos]=',';
			mb->pos++;

		    } else {

			first=0;

		    }

		    memcpy(&mb->data[mb->pos], algos[ctr].sshname, len);
		    mb->pos+=len;

		}

		ctr++;

	    }

	}

    } else {

	/* when not writing */

	while (algos[ctr].type>=0) {

	    if (algos[ctr].type==type) {

		if (first==0) {

		    mb->pos++;

		} else {

		    first=0;

		}

		mb->pos+=strlen(algos[ctr].sshname);

	    }

	    ctr++;

	}

    }

}

static int _send_kexinit(struct msg_buffer_s *mb, struct ssh_connection_s *connection)
{
    struct ssh_keyexchange_s *kex=&connection->setup.phase.transport.type.kex;
    char randombytes[16];
    struct kexinit_helper_s helper;

    logoutput_debug("_send_kexinit");

    helper.algos=kex->algos;
    helper.type=0;

    msg_write_byte(mb, SSH_MSG_KEXINIT);

    if (mb->data) {

	fill_random(randombytes, 16);

    } else {

	memset(randombytes, 0, 16);

    }

    msg_write_bytes(mb, (unsigned char *) randombytes, 16);

    for (unsigned int i=0; i<SSH_ALGO_TYPES_COUNT; i++) {

	helper.type=i; /* i = SSH_ALGO_TYPE_... from 0 to SSH_ALGO_TYPES_COUNT*/
	msg_fill_commalist(mb, build_kexinit_commalist, (void *) &helper);

    }

    msg_write_byte(mb, 0); /* first kexinit packet follows */
    msg_store_uint32(mb, 0); /* future use */
    return mb->pos;

}

int send_kexinit_message(struct ssh_connection_s *connection)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    int size=_send_kexinit(&mb, connection);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    struct ssh_keyexchange_s *kex=&connection->setup.phase.transport.type.kex;
    struct generic_error_s error=GENERIC_ERROR_INIT;

    init_ssh_payload(payload, size);
    set_msg_buffer_payload(&mb, payload);
    payload->len=_send_kexinit(&mb, connection);
    payload->type=SSH_MSG_KEXINIT;

    if (store_local_kexinit(kex, payload, (kex->flags & SSH_KEYEX_FLAG_SERVER), &error)==0) {

	return write_ssh_packet_kexinit(connection, payload);

    }

    return -1;

}

int send_newkeys_message(struct ssh_connection_s *connection)
{
    char buffer[sizeof(struct ssh_payload_s) + 1];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, 1);
    payload->type=SSH_MSG_NEWKEYS;
    payload->len=1;
    payload->buffer[0]=SSH_MSG_NEWKEYS;

    return write_ssh_packet(connection, payload);

}

static int _send_service_common_message(struct ssh_connection_s *connection, unsigned char code, const char *service)
{
    unsigned int size=strlen(service);
    char buffer[sizeof(struct ssh_payload_s) + 5 + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 5 + size);
    payload->type=code;

    *pos=code;
    pos++;

    store_uint32(pos, size);
    pos+=4;

    memcpy(pos, service, size);
    pos+=size;
    payload->len=(unsigned int)(pos - payload->buffer);

    return write_ssh_packet(connection, payload);

}

int send_service_request_message(struct ssh_connection_s *connection, const char *service)
{
    return _send_service_common_message(connection, SSH_MSG_SERVICE_REQUEST, service);
}

int send_service_accept_message(struct ssh_connection_s *connection, const char *service)
{
    return _send_service_common_message(connection, SSH_MSG_SERVICE_ACCEPT, service);
}

/* functions to send a global request
    (https://tools.ietf.org/html/rfc4254#section-4)
    a global request looks like:
    - byte			SSH_MSG_GLOBAL_REQUEST
    - string			request name
    - boolean			want reply
    - ....			request specific data

    for example:

    - request port forwarding
    (https://tools.ietf.org/html/rfc4254#section-7.1)
    - byte 			SSH_MSG_GLOBAL_REQUEST
    - string			"tcpip-forward"
    - boolean			want reply
    - string			address to bind (e.g., "0.0.0.0")
    - uint32			port number to bind
    */

int send_global_request_message(struct ssh_connection_s *connection, const char *service, char *data, unsigned int len)
{
    struct ssh_connections_s *connections=get_ssh_connection_connections(connection);
    unsigned int size=strlen(service);
    char buffer[sizeof(struct ssh_payload_s) + 5 + len + 1 + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 5 + len + 1 + size);
    payload->type=SSH_MSG_GLOBAL_REQUEST;

    *pos=SSH_MSG_GLOBAL_REQUEST;
    pos++;

    store_uint32(pos, size);
    pos+=4;

    memcpy(pos, service, size);
    pos+=len;

    *pos=1;
    pos++;

    if (data) {

	memcpy(pos, data, len);
	pos+=len;

    }

    payload->len=(unsigned int)(pos - payload->buffer);

    /* prevent more than one GLOBAL_REQUEST pending */

    signal_lock(connections->signal);

    while ((connections->flags & SSH_CONNECTIONS_FLAG_DISCONNECT)==0 && (connection->flags & SSH_CONNECTION_FLAG_GLOBAL_REQUEST)) {

	signal_condwait(connections->signal);

	if ((connection->flags & SSH_CONNECTION_FLAG_GLOBAL_REQUEST)==0) {

	    break;

	} else if (connections->flags & SSH_CONNECTIONS_FLAG_DISCONNECT) {

	    signal_unlock(connections->signal);
	    return -1;

	}

    }

    connection->flags |= SSH_CONNECTION_FLAG_GLOBAL_REQUEST;
    signal_unlock(connections->signal);

    return write_ssh_packet(connection, payload);
}

int send_request_success_message(struct ssh_connection_s *connection, char *data, unsigned int size)
{
    char buffer[sizeof(struct ssh_payload_s) + 5 + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 5 + size);
    payload->type=SSH_MSG_REQUEST_SUCCESS;

    *pos=SSH_MSG_REQUEST_SUCCESS;
    pos++;

    store_uint32(pos, size);
    pos+=4;

    memcpy(pos, data, size);
    pos+=size;
    payload->len=(unsigned int)(pos - payload->buffer);

    return write_ssh_packet(connection, payload);
}

int send_request_failure_message(struct ssh_connection_s *connection)
{
    char buffer[sizeof(struct ssh_payload_s) + 1];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, 1);
    payload->type=SSH_MSG_REQUEST_FAILURE;

    *pos=SSH_MSG_REQUEST_FAILURE;
    pos++;
    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(connection, payload);
}

static unsigned int _send_ext_info_message(struct msg_buffer_s *mb, unsigned int requested)
{
    unsigned int count=0;
    unsigned int pos=0;

    msg_write_byte(mb, SSH_MSG_EXT_INFO);

    pos=msg_start_count(mb);
    msg_store_uint32(mb, 0);

    if (requested & SSH_EXTENSION_SERVER_SIG_ALGS) {

	count++;
	msg_write_ssh_string(mb, 'c', (void *) get_extension_name(SSH_EXTENSION_SERVER_SIG_ALGS));

	/* string clist of pk algo's */

	/* where to get the supported pk sign algo's from? enumerate all the supported "non-default" sign algo's */

    }

    if (requested & SSH_EXTENSION_DELAY_COMPRESSION) {

	count++;
	msg_write_ssh_string(mb, 'c', (void *) get_extension_name(SSH_EXTENSION_DELAY_COMPRESSION));

	/* string of 
	    - clist comp algo's c2s
	    - clist comp algo's s2c
	*/

    }

    if (requested & SSH_EXTENSION_NO_FLOW_CONTROL) {

	count++;
	msg_write_ssh_string(mb, 'c', (void *) get_extension_name(SSH_EXTENSION_NO_FLOW_CONTROL));

	/* string of "p" (preferred) or "s" (supported) */

    }

    if (requested & SSH_EXTENSION_ELEVATION) {

	count++;
	msg_write_ssh_string(mb, 'c', (void *) get_extension_name(SSH_EXTENSION_ELEVATION));

	/* string "y" or "n" or "d" */
    }

    msg_complete_count(mb, pos, count);
    return mb->pos;
}

int send_ext_info_message(struct ssh_connection_s *connection, unsigned int requested)
{
    return -1;
}

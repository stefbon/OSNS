/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "libosns-misc.h"
#include "libosns-log.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-connections.h"
#include "ssh-send.h"
#include "ssh-receive.h"

extern struct fs_options_s fs_options;

static int select_payload_request_reply(struct ssh_payload_s *payload, void *ptr)
{
    return (payload->type==SSH_MSG_REQUEST_SUCCESS || payload->type==SSH_MSG_REQUEST_FAILURE) ? 1 : 0;
}

static void process_enum_supported_name(char *name, unsigned int len, void *ptr)
{
    struct ssh_connection_s *connection=(struct ssh_connection_s *) ptr;
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_extensions_s *extensions=&session->extensions;

    if (strcmp(name, "enum-supported@ssh.osns.net") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_ENUM_SUPPORTED;

    } else if (strcmp(name, "enum-services@ssh.osns.net") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_ENUM_SERVICES;

    } else if (strcmp(name, "info-service@ssh.osns.net") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_INFO_SERVICE;

    } else if (strcmp(name, "info-command@ssh.osns.net") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_INFO_COMMAND;

    } else if (strcmp(name, "udp-channel@ssh.osns.net") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_UDP_CHANNEL;

    } else if (strcmp(name, "tcpip-forward") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_TCPIP_FORWARD;

    } else if (strcmp(name, "cancel-tcpip-forward") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD;

    } else if (strcmp(name, "streamlocal-forward@openssh.com") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_STREAMLOCAL_FORWARD;

    } else if (strcmp(name, "cancel-streamlocal-forward@openssh.com") == 0) {

	extensions->global_requests |= SSH_GLOBAL_REQUEST_CANCEL_STREAMLOCAL_FORWARD;

    }

}

int process_global_request_message(struct ssh_connection_s *connection, char *request, unsigned char reply, char *data, unsigned int size, void (* cb)(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr), void *ptr)
{
    int result=-1;

    if (send_global_request_message(connection, request, data, size)==0) {
	struct system_timespec_s expire=SYSTEM_TIME_INIT;
	struct ssh_payload_s *payload=NULL;

	get_ssh_connection_expire_init(connection, &expire);

	getpayload:

	payload=get_ssh_payload(&connection->setup.queue, &expire, select_payload_request_reply, NULL, NULL, NULL);

	if (payload==NULL) {

	    logoutput("process_global_request_message: no payload received");

	} else if (payload->type==SSH_MSG_REQUEST_SUCCESS || payload->type==SSH_MSG_REQUEST_FAILURE) {

	    (* cb)(connection, payload, ptr);
	    result=0;

	} else {

	    free_payload(&payload);
	    goto getpayload;

	}

    } else {

	logoutput("process_global_request_message: failed to send request");

    }

    return result;
}

void enum_supported_global_requests_cb(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{

    if (payload->type==SSH_MSG_REQUEST_SUCCESS) {

	/* reply is a commaseparated list like:
	enum-supported@osns.net,enum-services@osns.net,info-service@osns.net*/

	if (payload->len>5) {
	    char *pos=&payload->buffer[1];
	    unsigned int size=0;

	    size=get_uint32(pos);
	    pos+=4;

	    if ((unsigned int)(pos - payload->buffer) + size <= payload->len) {

		logoutput("enum_supported_global_requests_cb: received %.*s", size, pos);
		parse_ssh_commalist(pos, size, process_enum_supported_name, (void *)connection);

	    } else {

		logoutput("enum_supported_global_requests_cb: received invalid message");

	    }

	} else {

	    logoutput("enum_supported_global_requests_cb: message received too small (%i)", payload->len);

	}

    } else if (payload->type==SSH_MSG_REQUEST_FAILURE) {

	logoutput("enum_supported_global_requests_cb: request not supported");

    }

}

void find_globalrequests_supported(struct ssh_connection_s *connection)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_extensions_s *extensions=&session->extensions;
    unsigned int index=SSH_EXTENSION_GR_SUPPORT - 1;

    if ((session->config.extensions & (2 * index))==0) {

	logoutput("find_globalrequests_supported: finding global requests not supported through local config");
	return;

    } else if ((session->config.extensions & (1 << ( 2 * index + 1)))==0) {

	logoutput("find_globalrequests_supported: finding global requests not supported by server");
	return;

    }

    /* send a global_request : supported message
	and process the MSG_REQUEST_SUCCESS payload*/

    if (process_global_request_message(connection, "enum-supported@osns.net", 0, NULL, 0, enum_supported_global_requests_cb, NULL)==0) {

	logoutput("find_globalrequests_supported: success");

    } else {

	logoutput("find_globalrequests_supported: failed");

    }

}


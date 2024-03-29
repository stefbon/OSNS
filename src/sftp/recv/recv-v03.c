/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/request-hash.h"
#include "sftp/protocol-v03.h"
#include "datatypes/ssh-uint.h"

/*
    SFTP callbacks
    sftp is encapsulated in SSH_MSG_CHANNEL_DATA
    so these functions are called when receiving am message of above type

    format for sftp data (except SSH_FXP_VERSION) :

    - uint32			length minus length field self 
    - byte			type
    - uint32			request-id
    - ... type specific fields ...

    (see: draft-ietf-secsh-filexfer 4. General Packet Format)

    when receiving the SSH_MSG_CHANNEL_DATA, the lenght and the type fields are already read
    and set in the sftp_header
    the buffer is the rest

*/

static unsigned int linux_error_map[] = {
    [SSH_FX_OK]				= 0,
    [SSH_FX_EOF]			= ENODATA,
    [SSH_FX_NO_SUCH_FILE]		= ENOENT,
    [SSH_FX_PERMISSION_DENIED]  	= EPERM,
    [SSH_FX_FAILURE]			= EIO,
    [SSH_FX_BAD_MESSAGE]		= EBADMSG,
    [SSH_FX_NO_CONNECTION]		= ENOTCONN,
    [SSH_FX_CONNECTION_LOST]		= ESHUTDOWN,
    [SSH_FX_OP_UNSUPPORTED]		= EOPNOTSUPP};


static unsigned int map_sftp_error(unsigned int ssh_fx_error)
{
    if (ssh_fx_error < (sizeof(linux_error_map)/sizeof(linux_error_map[0]))) return linux_error_map[ssh_fx_error];
    return EIO;
}


/*
    functions to handle "normal" replies from sftp

    there are only 5 different replies:
    - status
    - handle
    - data
    - name
    - attr

    the common values for these replies like:

    - byte	type
    - uint32	request id

    are stored in sftp_header

    the rest is in buffer
*/

void receive_sftp_status_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    struct generic_error_s error = GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	char *buffer=header->buffer;
	unsigned int pos=0;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->response.status.code=get_uint32(&buffer[pos]);
	reply->response.status.linux_error=map_sftp_error(reply->response.status.code);
	signal_sftp_received_id(sftp, req);

    } else {

	logoutput("receive_sftp_status_v03: error finding id %u (%s)", header->id, get_error_description(&error));

    }

}

void receive_sftp_handle_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	char *buffer=header->buffer;
	unsigned int pos=0;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->size=get_uint32(&buffer[pos]);
	pos+=4;

	if (reply->size <= SFTP_HANDLE_MAXSIZE && reply->size + pos <= header->len) {

	    memmove(buffer, &buffer[pos], reply->size);
	    reply->data=buffer;
	    signal_sftp_received_id(sftp, req);
	    header->buffer=NULL;

	} else {

	    set_generic_error_application(&error, _ERROR_APPLICATION_TYPE_PROTOCOL, NULL, __PRETTY_FUNCTION__);
	    logoutput("receive_sftp_handle_v03: error received handle len %i too long (%s)", reply->size, get_error_description(&error));
	    signal_sftp_received_id_error(sftp, req, &error);

	}

    } else {

	logoutput("receive_sftp_handle_v03: error finding id %i (%s)", header->id, get_error_description(&error));

    }

}

void receive_sftp_data_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	char *buffer=header->buffer;
	unsigned int pos=0;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->response.data.flags=0;
	reply->size=get_uint32(&buffer[pos]);
	pos+=4;
	memmove(buffer, &buffer[pos], reply->size);
	reply->data=buffer; /* let the processing of this into names, attr to the caller/initiator */
	header->buffer=NULL;
	signal_sftp_received_id(sftp, req);

    } else {

	logoutput("receive_sftp_data_v03: error finding id %u (%s)", header->id, get_error_description(&error));

    }

}

void receive_sftp_name_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	char *buffer=header->buffer;
	unsigned int pos=0;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->response.name.flags=0;
	reply->response.name.count=get_uint32(&buffer[pos]);
	pos+=4;
	reply->size=header->len - pos; /* minus the count field */
	memmove(buffer, &buffer[pos], reply->size);
	reply->data=buffer; /* let the processing of this into names, attr to the receiving (FUSE) thread */
	header->buffer=NULL;

	signal_sftp_received_id(sftp, req);

    } else {

	logoutput("receive_sftp_name_v03: error finding id %u (%s)", header->id, get_error_description(&error));

    }

}

void receive_sftp_attr_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	char *buffer=header->buffer;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->size=header->len;
	reply->data=buffer;
	header->buffer=NULL;
	signal_sftp_received_id(sftp, req);

    } else {

	logoutput("receive_sftp_attr_v03: error finding id %u (%s)", header->id, get_error_description(&error));

    }

}

void receive_sftp_extension_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
}

void receive_sftp_extension_reply_v03(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	unsigned int pos=0;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->size=header->len;
	reply->data=header->buffer;
	header->buffer=NULL;
	signal_sftp_received_id(sftp, req);

    } else {

	logoutput("receive_sftp_extension_reply_v03: error finding id %u (%s)", header->id, get_error_description(&error));

    }

}

static struct sftp_recv_ops_s recv_ops_v03 = {
    .status				= receive_sftp_status_v03,
    .handle				= receive_sftp_handle_v03,
    .data				= receive_sftp_data_v03,
    .name				= receive_sftp_name_v03,
    .attr				= receive_sftp_attr_v03,
    .extension				= receive_sftp_extension_v03,
    .extension_reply			= receive_sftp_extension_reply_v03,
};

struct sftp_recv_ops_s *get_sftp_recv_ops_v03()
{
    return &recv_ops_v03;
}

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
#include "sftp/protocol-v05.h"
#include "recv-v03.h"
#include "recv-v04.h"
#include "datatypes/ssh-uint.h"

static unsigned int linux_error_map[] = {
    [SSH_FX_OK]				= 0,
    [SSH_FX_EOF]			= ENODATA,
    [SSH_FX_NO_SUCH_FILE]		= ENOENT,
    [SSH_FX_PERMISSION_DENIED]  	= EPERM,
    [SSH_FX_FAILURE]			= EIO,
    [SSH_FX_BAD_MESSAGE]		= EBADMSG,
    [SSH_FX_NO_CONNECTION]		= ENOTCONN,
    [SSH_FX_CONNECTION_LOST]		= ESHUTDOWN,
    [SSH_FX_OP_UNSUPPORTED]		= EOPNOTSUPP,
    [SSH_FX_INVALID_HANDLE]		= EINVAL,
    [SSH_FX_NO_SUCH_PATH]		= ENOENT,
    [SSH_FX_FILE_ALREADY_EXISTS]	= EEXIST,
    [SSH_FX_WRITE_PROTECT]		= EACCES,
    [SSH_FX_NO_MEDIA]			= ENODEV,
    [SSH_FX_NO_SPACE_ON_FILESYSTEM]	= ENOSPC,
    [SSH_FX_QUOTA_EXCEEDED]		= EDQUOT,
    [SSH_FX_UNKNOWN_PRINCIPAL]		= EIO,
    [SSH_FX_LOCK_CONFLICT]		= EWOULDBLOCK};

static unsigned int map_sftp_error_v05(unsigned int ssh_fx_error)
{
    if (ssh_fx_error < (sizeof(linux_error_map)/sizeof(linux_error_map[0]))) return linux_error_map[ssh_fx_error];
    return EIO;
}

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

void _receive_sftp_status_v05(struct sftp_client_s *sftp, struct sftp_header_s *header, unsigned int (* map_error)(unsigned int status))
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    if ((req=get_sftp_request(sftp, header->id, &error))) {
	char *buffer=header->buffer;
	unsigned int len=0;
	unsigned int pos=0;
	struct sftp_reply_s *reply=&req->reply;

	reply->type=header->type;
	reply->response.status.code=get_uint32(&buffer[pos]);
	reply->response.status.linux_error=map_error(reply->response.status.code);

	pos+=4;
	len=get_uint32(&buffer[pos]);
	pos+=4;

	if (len>0) {
	    char errormessage[len+1];

	    memcpy(errormessage, &buffer[pos], len);
	    errormessage[len]='\0';
	    pos+=len;
	    logoutput("receive_sftp_status_v05: error %i:%s", reply->response.status.code, errormessage);

	} else {

	    logoutput("receive_sftp_status_v05: error %i", reply->response.status.code);

	}

	if (pos + 4 < header->len) {

	    /* language tag for error message (ignore) */

	    len=get_uint32(&buffer[pos]);
	    pos+=4;

	    if (pos + len < header->len) {

		pos+=len;

		/* error specific data */

		reply->size=(header->len - pos);
		memmove(buffer, &buffer[pos], reply->size);
		reply->data=buffer;
		header->buffer=NULL;

	    }

	}

	signal_sftp_received_id(sftp, req);

    } else {

	logoutput("receive_sftp_status_v05: error finding id %i (%s)", header->id, get_error_description(&error));

    }

}

void receive_sftp_status_v05(struct sftp_client_s *sftp, struct sftp_header_s *header)
{
    _receive_sftp_status_v05(sftp, header, map_sftp_error_v05);
}

static struct sftp_recv_ops_s recv_ops_v05 = {
    .status				= receive_sftp_status_v05,
    .handle				= receive_sftp_handle_v03,
    .data				= receive_sftp_data_v03,
    .name				= receive_sftp_name_v03,
    .attr				= receive_sftp_attr_v03,
    .extension				= receive_sftp_extension_v03,
    .extension_reply			= receive_sftp_extension_reply_v03,
};

struct sftp_recv_ops_s *get_sftp_recv_ops_v05()
{
    return &recv_ops_v05;
}

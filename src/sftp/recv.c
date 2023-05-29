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
#include "common.h"
#include "request-hash.h"
#include "datatypes/ssh-uint.h"
#include "recv/recv-v03.h"
#include "recv/recv-v04.h"
#include "recv/recv-v05.h"
#include "recv/recv-v06.h"

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

static int signal_sftp_reply_invalid(struct sftp_client_s *sftp, uint32_t id, unsigned char type)
{
    struct generic_error_s error = GENERIC_ERROR_INIT;
    struct sftp_request_s *req=NULL;

    req=get_sftp_request(sftp, id, &error);

    if (req) {

	req->reply.type=type;
	signal_sftp_received_id_error(sftp, req, &error);
	return 0;

    }

    return -1;

}

static void process_receive_sftp_reply_error(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t seq)
{
    struct sftp_signal_s *signal=&sftp->signal;
    unsigned char type=0;

    /* something is wrong with message: try to send a signal to waiting thread */

    if (size>=9) {
	unsigned int pos=4; /* start at the type byte */
	uint32_t id=0;

	type=(unsigned char) buffer[pos];
	pos++;
	id=get_uint32(&buffer[pos]);
	pos+=4;

	if (signal_sftp_reply_invalid(sftp, id, type)==0) return;

    }

    /* request not found: fallback on seq */

    type=((size>=5) ? ((unsigned char) buffer[4]) : 0);

    /* not able to get the unique sftp id .... rely on the sequence number to communcate with waiting thread */

    signal_lock(signal->signal);
    signal->seq=seq;
    signal->seqtype=type;
    get_current_time_system_time(&signal->seqset);
    set_generic_error_application(&signal->error, _ERROR_APPLICATION_TYPE_PROTOCOL, NULL, __PRETTY_FUNCTION__);
    signal_broadcast(signal->signal);
    signal_unlock(signal->signal);

}

static void _receive_sftp_reply(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t seq, unsigned int flags)
{
    struct sftp_header_s header;
    unsigned int pos=0;
    unsigned int len=0;

    if (size<9) {

	logoutput("receive_sftp_reply: received size %i too small", size);
	process_receive_sftp_reply_error(sftp, buffer, size, seq);
	return;

    }

    len=get_uint32(&buffer[pos]);
    pos+=4;

    /*
	SFTP has the form
	- uint32		length minus this field
	- byte			type
	- uint32		request id
	- type specific data

	(see: https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13#section-4 )
    */

    /* length of sftp data plus 4 is equal to the length of the packet
	extra data should be ignored: it is allowed that the packet size is bigger than
	the length plus 4  */

    if (4 + len > size) {

	logoutput("receive_sftp_reply: received size %i smaller than sftp length %i", size, 4 + len);
	process_receive_sftp_reply_error(sftp, buffer, size, seq);
	return;

    }

    header.len=len;
    header.buffer=NULL;
    header.id=0;
    header.seq=seq;

    header.type=(unsigned char) buffer[pos];
    pos++;
    header.len--;

    header.id=get_uint32(&buffer[pos]);
    pos+=4;
    header.len-=4;

    memmove(buffer, &buffer[pos], header.len); /* move out the information not relevant to the callbacks */
    memset(&buffer[header.len], 0, size - header.len); /* clear the area which is of no use anymore */

    header.buffer=buffer; /* sftp takes over the buffer */
    logoutput_debug("receive_sftp_reply: type %u size %i", header.type, header.len);

    switch (header.type) {

	case SSH_FXP_STATUS:

	    (* sftp->recv_ops->status)(sftp, &header);
	    break;

	case SSH_FXP_HANDLE:

	    (* sftp->recv_ops->handle)(sftp, &header);
	    break;

	case SSH_FXP_DATA:

	    (* sftp->recv_ops->data)(sftp, &header);
	    break;

	case SSH_FXP_NAME:

	    (* sftp->recv_ops->name)(sftp, &header);
	    break;

	case SSH_FXP_ATTRS:

	    (* sftp->recv_ops->attr)(sftp, &header);
	    break;

	case SSH_FXP_EXTENDED:

	    (* sftp->recv_ops->extension)(sftp, &header);
	    break;

	case SSH_FXP_EXTENDED_REPLY:

	    (* sftp->recv_ops->extension_reply)(sftp, &header);
	    break;

	default:

	    if (signal_sftp_reply_invalid(sftp, header.id, header.type)==0) {

		logoutput("receive_sftp_reply: error sftp type %i not reckognized (request informed)", header.type);

	    } else {

		logoutput("receive_sftp_reply: error sftp type %i not reckognized (request not found)", header.type);

	    }

	    break;

    }

}

static void _receive_sftp_init(struct sftp_client_s *sftp, char *buffer, unsigned int size, unsigned int seq, unsigned int flags)
{

    sftp->receive.receive_data=_receive_sftp_reply;

    if (size>=5) {
	uint32_t len=0;
	char *pos=buffer;
	unsigned char type=0;

	len=get_uint32(pos);
	pos+=4;
	type=(unsigned char) *pos;
	pos++;
	len--;

	logoutput("_receive_sftp_init: received len %i size %i type %i", len, size, type);

	if (5 + len <= size && type==SSH_FXP_VERSION) {
	    struct generic_error_s error = GENERIC_ERROR_INIT;
	    struct sftp_request_s *req=NULL;

	    memmove(buffer, pos, len);

	    /* look for the request with the special id (uint32_t) -1 since
		the init and version messages do not have an id a custom one is used */

	    if ((req=get_sftp_request(sftp, (uint32_t) -1, &error))) {
		struct sftp_reply_s *reply=&req->reply;

		logoutput("_receive_sftp_init: type %i req found", type);

		reply->type=type;
		reply->size=len;
		reply->data=buffer;

		signal_sftp_received_id(sftp, req);

	    } else {

		logoutput("_receive_sftp_init: error finding init (%s)", get_error_description(&error));
		signal_sftp_received_id_error(sftp, req, &error);

	    }

	}

    } else {

	logoutput("_receive_sftp_init: protocol error");
	process_receive_sftp_reply_error(sftp, *buffer, size, seq);

    }

}

void receive_sftp_data(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t seq, unsigned int flags)
{
    pthread_mutex_lock(&sftp->mutex);
    (* sftp->receive.receive_data)(sftp, buffer, size, seq, flags);
    pthread_mutex_unlock(&sftp->mutex);
}

void init_sftp_receive(struct sftp_client_s *sftp)
{
    sftp->receive.receive_data=_receive_sftp_init;
}

void switch_sftp_receive(struct sftp_client_s *sftp, const char *what)
{
    pthread_mutex_lock(&sftp->mutex);

    if (strcmp(what, "init")==0) {

	sftp->receive.receive_data=_receive_sftp_init;

    } else if (strcmp(what, "session")==0) {

	sftp->receive.receive_data=_receive_sftp_reply;

    }

    pthread_mutex_unlock(&sftp->mutex);
}

void set_sftp_recv_version(struct sftp_client_s *sftp)
{
    unsigned char version=sftp->protocol.version;

    if (version==3) {

	sftp->recv_ops=get_sftp_recv_ops_v03();

    } else if (version==4) {

	sftp->recv_ops=get_sftp_recv_ops_v04();

    } else if (version==5) {

	sftp->recv_ops=get_sftp_recv_ops_v05();

    } else if (version==6) {

	sftp->recv_ops=get_sftp_recv_ops_v06();

    } else {

	logoutput_warning("set_sftp_recv_version: error sftp protocol version not suuported", version);


    }

}


/*
  2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-connections.h"

#include "ssh-utils.h"

#include "userauth/utils.h"

int send_userauth_none_request(struct ssh_connection_s *connection, struct ssh_string_s *service)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    int result=-1;

    /* get the list of authemtication 'method name' values
	see https://tools.ietf.org/html/rfc4252#section-5.2: The "none" Authentication Request
    */

    logoutput("send_auth_none: send none userauth request");

    /* send with the client username since the username used to connect is not known here
	this username will follow after pubkey when the different pubkeys/user are offered */

    if (send_userauth_none_message(connection, &auth->c_username, service)>0) {
	struct ssh_payload_s *payload=NULL;

	payload=receive_message_common(connection, select_userauth_reply, NULL);
	if (payload==NULL) goto finish;

	if (payload->type == SSH_MSG_USERAUTH_SUCCESS) {

	    /* huhh?? which server allows this weak security? */
	    logoutput("send_auth_none: server accepted none.....");
	    result=0;

	} else if (payload->type == SSH_MSG_USERAUTH_FAILURE) {

	    /* result will always be -1 since "none" will result in success
		override this */
	    handle_auth_failure(payload, auth);
	    result=0;

	} else {

	    logoutput("send_userauth_none: got unexpected reply %i", payload->type);
	    goto finish;

	}

	if (payload) free_payload(&payload);

    } else {

	/* why send error ?*/

	logoutput("send_userauth_none: error sending SSH_MSG_USERAUTH_REQUEST");

    }

    finish:
    return result;

}

int respond_userauth_none_request(struct ssh_connection_s *connection, struct ssh_string_s *username, struct ssh_string_s *service, struct ssh_string_s *data, struct system_timespec_s *expire)
{
    return respond_userauth_request(connection, 0);
}

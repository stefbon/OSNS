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
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-hostinfo.h"
#include "ssh-utils.h"
#include "userauth/utils.h"
#include "users/check.h"

static const char *get_credentials_scope(unsigned char type)
{
    if (type==PW_TYPE_GLOBAL) {

	return "global";

    } else if (type==PW_TYPE_DOMAIN) {

	return "domain";

    } else if (type==PW_TYPE_HOSTNAME) {

	return "hostname";

    } else if (type==PW_TYPE_IPV4) {

	return "ipv4";

    }

    return "unknown";

}

int send_userauth_password_request(struct ssh_connection_s *connection, struct ssh_string_s *service, struct pw_list_s *pwlist)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct pw_list_s *list=NULL;
    int result=-1;

    logoutput("ssh_auth_password");

    list=get_next_pwlist(pwlist, list);

    while (list) {

	logoutput("ssh_auth_password: send user and password for %s (scope: %s)", list->pword.user, get_credentials_scope(list->type));

	if (send_userauth_password_message(connection, list->pword.user, list->pword.pw, service)>0) {
	    struct ssh_payload_s *payload=NULL;

	    payload=receive_message_common(connection, select_userauth_reply, NULL);
	    if (payload==NULL) goto out;

	    if (payload->type == SSH_MSG_USERAUTH_SUCCESS) {

		logoutput("ssh_auth_password: success");
		auth->required=0;
		result=0;

	    } else if (payload->type == SSH_MSG_USERAUTH_FAILURE) {

		logoutput("ssh_auth_password: failed");
		result=handle_auth_failure(payload, auth);

	    }

	    free_payload(&payload);

	} else {

	    logoutput("ssh_auth_password: error sending SSH_MSG_SERVICE_REQUEST packet");

	}

	list=get_next_pwlist(pwlist, list);

    }

    out:
    return result;

}

int respond_userauth_password_request(struct ssh_connection_s *connection, struct ssh_string_s *username, struct ssh_string_s *service, struct ssh_string_s *data, struct system_timespec_s *expire)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    unsigned char success=0;

    if ((auth->required & SSH_AUTH_METHOD_PASSWORD)==0) return respond_userauth_request(connection, 1);

    if (check_username_password(username, NULL)==-1) {

	logoutput("respond_userauth_password_request: username not found; cannot continue");
	return respond_userauth_request(connection, 0);
    }

    if (data->len>1) {
	char *buffer=data->ptr;
	unsigned int pos=0;
	struct ssh_string_s password=SSH_STRING_INIT;

	if (read_ssh_string(&buffer[1], data->len-1, &password)>4) {

	    if (check_username_password(username, &password)==0) {

		auth->required &= ~SSH_AUTH_METHOD_PASSWORD;
		return respond_userauth_request(connection, 1);

	    }

	}

    }

    /* when here passwd auth has failed */
    return respond_userauth_request(connection, 0);

}

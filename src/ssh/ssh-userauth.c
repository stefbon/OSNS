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
#include "libosns-network.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"

#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-connections.h"
#include "ssh-utils.h"
// #include "users.h"

#include "userauth/pubkey.h"
#include "userauth/hostbased.h"
#include "userauth/utils.h"
#include "userauth/none.h"
#include "userauth/password.h"

void init_ssh_auth(struct ssh_auth_s *auth)
{
    memset(auth, 0, sizeof(struct ssh_auth_s));

    auth->required=0;
    auth->done=0;

    init_ssh_string(&auth->c_hostname);
    init_ssh_string(&auth->c_ip);
    init_ssh_string(&auth->c_username);
    init_ssh_string(&auth->s_username);
}

void clear_ssh_auth(struct ssh_auth_s *auth)
{
    struct ssh_string_s *tmp=NULL;

    tmp=&auth->c_hostname;
    free_ssh_string(&tmp);

    tmp=&auth->c_ip;
    free_ssh_string(&tmp);

    tmp=&auth->c_username;
    free_ssh_string(&tmp);

    tmp=&auth->s_username;
    free_ssh_string(&tmp);

}

static int ssh_auth_method_supported(unsigned int methods)
{
    methods &= ~(SSH_AUTH_METHOD_NONE | SSH_AUTH_METHOD_PUBLICKEY | SSH_AUTH_METHOD_HOSTBASED | SSH_AUTH_METHOD_PASSWORD);
    return (methods > 0) ? -1 : 0;
}

static unsigned int get_required_userauth_methods(struct ssh_session_s *session)
{
    unsigned int methods;
    /* which services are required ?*/

    if (session->config.auth & SSH_CONFIG_AUTH_PASSWORD) methods |= SSH_AUTH_METHOD_PASSWORD;
    if (session->config.auth & SSH_CONFIG_AUTH_PUBLICKEY) methods |= SSH_AUTH_METHOD_PUBLICKEY;
    if (session->config.auth & SSH_CONFIG_AUTH_HOSTKEY) methods |= SSH_AUTH_METHOD_HOSTBASED;
    return methods;

}

static int start_ssh_userauth_client(struct ssh_session_s *session, struct ssh_connection_s *connection)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct ssh_string_s service=SSH_STRING_INIT;
    struct pk_list_s pkeys;
    struct pk_identity_s *user_identity=NULL;
    struct pk_identity_s *host_identity=NULL;
    char *user=NULL;
    struct network_peer_s local;

    logoutput("start_ssh_userauth_client");

    init_list_public_keys(&session->identity.pwd, &pkeys);

    if (request_ssh_service(connection, "ssh-userauth")==-1) {

	logoutput("start_ssh_userauth_client: request for ssh userauth failed");
	return -1;

    }

    /* client username */

    user=session->identity.pwd.pw_name;
    set_ssh_string(&auth->c_username, 'c', user);
    set_ssh_string(&service, 'c', "ssh-connection");

    /* get the list of authemtication 'method name' values
	see https://tools.ietf.org/html/rfc4252#section-5.2: The "none" Authentication Request
	note the remote user is set as the local user since the remote user is not known here */

    if (send_userauth_none_request(connection, &service)==-1) {

	logoutput("start_ssh_userauth_client: send userauth none failed");
	return -1;

    } else {

	if (auth->required == 0) {

	    /* huh?? no futher methods required */
	    goto finish;

	} else if (ssh_auth_method_supported(auth->required)==-1) {

	    /* not supported userauth methods requested by server */
	    goto finish;

	}

    }

    memset(&local, 0, sizeof(struct network_peer_s));
    local.host.flags=(HOST_ADDRESS_FLAG_IP | HOST_ADDRESS_FLAG_HOSTNAME);

    if (get_network_peer_properties(&connection->connection.sock, &local, "local")==-1) {

	logoutput("start_ssh_userauth_client: not able to get local address");
	goto finish;

    }

    set_ssh_string(&auth->c_hostname, 'c', local.host.hostname);

    if (local.host.ip.family==SYSTEM_SOCKET_FLAG_IPv4) {

	set_ssh_string(&auth->c_ip, 'c', local.host.ip.addr.v4);

    } else if (local.host.ip.family==SYSTEM_SOCKET_FLAG_IPv6) {

	set_ssh_string(&auth->c_ip, 'c', local.host.ip.addr.v6);

    }

    tryuserauth:
    logoutput("start_ssh_userauth_client: d:r %i:%i)", auth->done, auth->required);

    /* 	try publickey first if required, assume the order of methods does not matter to the server */

    if (auth->required & SSH_AUTH_METHOD_PUBLICKEY) {
	char *s_user=NULL;
	char *file=NULL;

	if (auth->done & SSH_AUTH_METHOD_PUBLICKEY) {

	    /* prevent cycles */

	    logoutput("start_ssh_userauth_client: publickey auth failed: cycles detected");
	    goto finish;

	}

	logoutput("start_ssh_auth_client: starting pk auth");

	/* get list of pk keys from local openssh user files */

	if (populate_list_public_keys(&pkeys, PK_IDENTITY_SOURCE_OPENSSH_LOCAL, "user")==0) {

	    logoutput("start_ssh_userauth_client: list of public keys is empty");
	    goto finish;
	}

	user_identity=send_userauth_pubkey_request(connection, &service, &pkeys);

	if (user_identity==NULL) {

	    /* pubkey userauth should result in at least one pk identity */

	    logoutput("start_ssh_userauth_client: no pk identity found");
	    goto finish;

	}

	/* TODO: store the values user and key/filename in auth */

	user=get_pk_identity_user(user_identity);
	if (user==NULL) user=session->identity.pwd.pw_name;
	if (ssh_string_isempty(&auth->s_username)) set_ssh_string(&auth->s_username, 'c', user);

	file=get_pk_identity_file(user_identity);
	logoutput("start_ssh_userauth_client: pk userauth success (d:r %i:%i) with file %s (user %s source %s)", 
		auth->done, auth->required, (file ? file : "unknown"), user, get_pk_identity_source(user_identity));

	auth->done|=SSH_AUTH_METHOD_PUBLICKEY;

    } else if (auth->required & SSH_AUTH_METHOD_PASSWORD) {
	struct pw_list_s *pwlist=NULL;

	if (auth->done & SSH_AUTH_METHOD_PASSWORD) {

	    /* prevent cycles */

	    logoutput("start_ssh_userauth_client: password auth failed: cycles detected");
	    goto finish;

	}

	logoutput("start_ssh_userauth_client: starting password auth");

	/* get list of pk keys from local openssh user files */

	if (read_private_pwlist(connection, &pwlist)==0) goto finish;

	if (send_userauth_password_request(connection, &service, pwlist)==-1) {

	    logoutput("start_ssh_userauth_client: passwd failed");
	    free_pwlist(pwlist);
	    goto finish;

	}

	auth->done|=SSH_AUTH_METHOD_PASSWORD;
	logoutput("start_ssh_userauth_client: password auth success");
	free_pwlist(pwlist);

    } else if (auth->required & SSH_AUTH_METHOD_HOSTBASED) {
	char *s_user=NULL;

	if (auth->done & SSH_AUTH_METHOD_HOSTBASED) {

	    /* prevent cycles */

	    logoutput("start_ssh_userauth_client: hostbased auth failed: cycles detected");
	    goto finish;

	}

	if (populate_list_public_keys(&pkeys, PK_IDENTITY_SOURCE_OPENSSH_LOCAL, "host")==0) goto finish;
	host_identity=send_userauth_hostbased_request(connection, &service, &pkeys);

	if (host_identity==NULL) {

	    /* hostbased userauth should result in at least one pk identity */

	    logoutput("start_ssh_userauth_client: hostbased failed/no identity found");
	    goto finish;

	}

	logoutput("start_ssh_userauth_client: hostbased success (d:r %i:%i)", auth->done, auth->required);
	auth->done|=SSH_AUTH_METHOD_HOSTBASED;

    }

    if (auth->required==0) {

	logoutput("start_ssh_userauth_client: no more methods required");

    } else if (ssh_auth_method_supported(auth->required)==-1) {

	logoutput("start_ssh_userauth_client: methods not supported");

    } else {

	goto tryuserauth;

    }

    finish:

    if (auth->required==0) {
	struct ssh_string_s *remote_user=&session->identity.remote_user;

	if (get_ssh_string_length(&auth->s_username, SSH_STRING_FLAG_DATA)==0) {
	    char *s_user=NULL;

	    if (user_identity) s_user=get_pk_identity_user(user_identity);
	    if (s_user) set_ssh_string(&auth->s_username, 'c', s_user);

	}

	create_ssh_string(&remote_user, auth->s_username.len, auth->s_username.ptr, SSH_STRING_FLAG_ALLOC);
	logoutput("start_ssh_userauth_client: remote user %.*s", remote_user->len, remote_user->ptr);

    }

    /* also do something with the public key file ? */

    if (host_identity) free(host_identity);
    if (user_identity) free(user_identity);
    free_lists_public_keys(&pkeys);
    return (auth->required==0) ? 0 : -1;

}

static int select_service_request_message(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    return (payload->type==SSH_MSG_SERVICE_REQUEST) ? 0 : -1;
}

static int select_userauth_request_message(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    return (payload->type==SSH_MSG_USERAUTH_REQUEST) ? 0 : -1;
}

static int start_ssh_userauth_server(struct ssh_session_s *session, struct ssh_connection_s *connection)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct ssh_payload_s *payload=NULL;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct system_timespec_s expire=SYSTEM_TIME_INIT;
    uint32_t seq=0;
    int result=-1;
    char *buffer=payload->buffer;
    unsigned int size=payload->len;
    struct ssh_string_s service1=SSH_STRING_INIT;

    logoutput("start_ssh_userauth_server");

    /* wait for SSH_MSG_SERVICE_REQUEST
	NOTE: the expire is crucial and for the whole process of the request to complete */

    get_ssh_connection_expire_userauth(connection, &expire);

    payload=get_ssh_payload(connection, &connection->setup.queue, &expire, &seq, select_service_request_message, NULL, &error);
    if (payload==NULL || payload->len<5) {

	logoutput("start_ssh_auth_server: not received a service request message or message is too small (%s)", get_error_description(&error));
	goto disconnect;

    }

    /* received a SSH_MSG_SERVICE_REQUEST */

    buffer=payload->buffer;
    size=payload->len;

    /* message looks like:
	- byte					SSH_MSG_SERVICE_REQUEST
	- string				service name */

    if (read_ssh_string(&buffer[1], size-1, &service1)<5) {

	logoutput("start_ssh_auth_server: not able to read service from service request");
	goto disconnect;

    }

    /* get/set the required methods from config
	maybe make this depend on the host/user */

    auth->required=get_required_userauth_methods(session);
    auth->done=0;

    if (compare_ssh_string(&service1, 'c', "ssh-userauth")==0) {
	struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
	struct ssh_string_s username=SSH_STRING_INIT;
	struct ssh_string_s service=SSH_STRING_INIT;
	struct ssh_string_s method=SSH_STRING_INIT;
	struct ssh_string_s data=SSH_STRING_INIT;

	if (send_service_accept_message(connection, "ssh-userauth", &seq)==-1) {

	    logoutput("start_ssh_auth_server: unable to send service accept message for userauth");
	    goto disconnect;

	}

	/* TODO: send a SSH_MSG_USERAUTH_BANNER message ... see:
	    https://tools.ietf.org/html/rfc4252#section-5.4
	    saying:

	    "In some jurisdictions, sending a warning message before
		authentication may be relevant for getting legal protection."

	*/

	untiluserauthfinished:

	/* wait for ssh userauth messages, but howto set a global timeout??

	    message has form:
	    - byte			SSH_MSG_USERAUTH_REQUEST
	    - string			username to connect to server
	    - string 			service to start after auth finished successfully
	    - string			method {publickey, password, hostbased, none}
	    - method specific data */

	payload=get_ssh_payload(connection, &connection->setup.queue, &expire, &seq, select_service_request_message, NULL, &error);
	if (payload==NULL || payload->len<5) {

	    logoutput("start_ssh_auth_server: not received a service request message or message is too small (%s)", get_error_description(&error));
	    goto disconnect;

	}

	set_msg_buffer_payload(&mb, payload);
	msg_read_byte(&mb, NULL);
	msg_read_ssh_string(&mb, &username);
	msg_read_ssh_string(&mb, &service);
	msg_read_ssh_string(&mb, &method);

	/* check for errors */

	if (mb.error>0) {

	    set_generic_error_system(&error, mb.error, NULL);
	    logoutput("start_ssh_auth_server: error reading userauth request message (%s)", get_error_description(&error));
	    goto disconnect;

	}

	if (check_username_password(&username, NULL)==-1) {

	    logoutput("start_ssh_auth_server: username %.*s not found on server; cannot continue", username.len, username.ptr);
	    return respond_userauth_request(connection, 0);
	}

	data.len=(mb.len - mb.pos);
	data.ptr=&mb.data[mb.pos];

	if (compare_ssh_string(&method, 'c', "none")==0) {

	    result=respond_userauth_none_request(connection, &username, &service, &data, &expire);

	} else if (compare_ssh_string(&method, 'c', "password")==0) {

	    result=respond_userauth_password_request(connection, &username, &service, &data, &expire);

	} else if (compare_ssh_string(&method, 'c', "publickey")==0) {

	    result=respond_userauth_publickey_request(connection, &username, &service, &data, &expire);

	} else if (compare_ssh_string(&method, 'c', "hostbased")==0) {

	    result=respond_userauth_hostbased_request(connection, &username, &service, &data, &expire);

	} else {

	    logoutput("start_ssh_auth_server: service %.*s not reckognized", method.len, method.ptr);
	    goto disconnect;

	}

	if (auth->required > 0) goto untiluserauthfinished;

	if (result==0) {

	    /* after success look at the service requested */

	    if (compare_ssh_string(&service, 'c', "ssh-connection")==0) {

		result=SSH_SERVICE_TYPE_CONNECTION;

	    }

	}

    } else {

	logoutput("start_ssh_auth_server: unexpected service");
	goto disconnect;

    }

    return result;

    disconnect:
    logoutput("start_ssh_auth_server: cannot continue, disconnecting...");
    disconnect_ssh_connection(connection);
    return -1;

}

int start_ssh_userauth(struct ssh_session_s *session, struct ssh_connection_s *connection)
{

    logoutput("start_ssh_userauth");

    if (session->flags & SSH_SESSION_FLAG_SERVER) {

	return start_ssh_userauth_server(session, connection);

    }

    return start_ssh_userauth_client(session, connection);
}

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "log.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "pk/pk-types.h"

/*
    take care for userauth for this client
    since this is a fuse fs, the only acceptable methods are
    publickey and hostbased, where publickey is required

    password is not possible since there is no easy method
    available from the fuse fs to provide a user interface
    (maybe kerberos using the ticket is possible)

*/

/*
    send a public key auth request (RFC4252 7. Public Key Authentication Method: "publickey")

    - byte 	SSH_MSG_USERAUTH_REQUEST
    - string	username
    - string	service name
    - string	"publickey"
    - boolean	FALSE or TRUE: true when signature is defined
    - string	public key algorithm name (ssh-rsa, ssh-dss)
    - string	public key
    - string	signature

    signature is as follows:

    - uint32	length of total signature packet
    - uint32	length name signature algorithm name
    - byte[]	name algorithm
    - uint32	length signature blob
    - byte[]	signature blob
*/

static void _msg_write_userauth_pubkey_message(struct msg_buffer_s *mb, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    msg_write_byte(mb, SSH_MSG_USERAUTH_REQUEST);
    msg_write_ssh_string(mb, 's', (void *) s_username);
    msg_write_ssh_string(mb, 's', (void *) service);
    msg_write_ssh_string(mb, 'c', (void *) "publickey");
    msg_write_byte(mb, (signature) ? 1 : 0);
    msg_write_pkalgo(mb, pkey->algo);
    msg_write_pkey(mb, pkey, PK_DATA_FORMAT_SSH_STRING);
    msg_write_pksignature(mb, pkey->algo, signature);
}

static unsigned int _write_userauth_pubkey_message(struct msg_buffer_s *mb, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    _msg_write_userauth_pubkey_message(mb, s_username, service, pkey, signature);
    return mb->pos;
}

/* write the userauth request message to a buffer
    used for the creating of a signature with public key auth */

void msg_write_userauth_pubkey_request(struct msg_buffer_s *mb, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    _msg_write_userauth_pubkey_message(mb, s_username, service, pkey, signature);
}

int send_userauth_pubkey_message(struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *signature, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=_write_userauth_pubkey_message(&mb, s_username, service, pkey, signature) + 64;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_USERAUTH_REQUEST;
    set_msg_buffer_payload(&mb, payload);
    payload->len=_write_userauth_pubkey_message(&mb, s_username, service, pkey, signature);
    return write_ssh_packet(connection, payload, seq);
}

unsigned int write_userauth_pubkey_ok_message(struct msg_buffer_s *mb, struct ssh_key_s *pkey)
{
    msg_write_byte(mb, SSH_MSG_USERAUTH_PK_OK);
    msg_write_pkalgo(mb, pkey->algo);
    msg_write_pkey(mb, pkey, PK_DATA_FORMAT_SSH_STRING);
    return mb->pos;
}

int send_userauth_pubkey_ok_message(struct ssh_connection_s *connection, struct ssh_key_s *pkey, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=write_userauth_pubkey_ok_message(&mb, pkey) + 64;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_USERAUTH_PK_OK;
    set_msg_buffer_payload(&mb, payload);
    payload->len=write_userauth_pubkey_ok_message(&mb, pkey);
    return write_ssh_packet(connection, payload, seq);
}

/*
    send a hostbased auth request (RFC4252 5.2. The "none" Authentication Request)

    - byte 	SSH_MSG_USERAUTH_REQUEST
    - string	username used to connect
    - string	service name
    - string	"none"
*/

static void _msg_write_userauth_none_message(struct msg_buffer_s *mb, struct ssh_string_s *user, struct ssh_string_s *service)
{
    msg_write_byte(mb, SSH_MSG_USERAUTH_REQUEST);
    msg_write_ssh_string(mb, 's', (void *) user);
    msg_write_ssh_string(mb, 's', (void *) service);
    msg_write_ssh_string(mb, 'c', (void *) "none");
}

static unsigned int _write_userauth_none_message(struct msg_buffer_s *mb, struct ssh_string_s *user, struct ssh_string_s *service)
{
    _msg_write_userauth_none_message(mb, user, service);
    return mb->pos;
}

void msg_write_userauth_none_message(struct msg_buffer_s *mb, struct ssh_string_s *user, struct ssh_string_s *service)
{
    _msg_write_userauth_none_message(mb, user, service);
}

int send_userauth_none_message(struct ssh_connection_s *connection, struct ssh_string_s *user, struct ssh_string_s *service, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=_write_userauth_none_message(&mb, user, service);
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_USERAUTH_REQUEST;
    set_msg_buffer_payload(&mb, payload);
    payload->len=_write_userauth_none_message(&mb, user, service);
    return write_ssh_packet(connection, payload, seq);
}

/*
    send a hostbased auth request (RFC4252 9. Host-Based Authentication Method: "hostbased")

    - byte 	SSH_MSG_USERAUTH_REQUEST
    - string	username used to connect
    - string	service name
    - string	"hostbased"
    - string	public key algorithm name (ssh-rsa, ssh-dss, ...)
    - string	public host key client host
    - string	client hostname
    - string	local username
    - string	signature
*/

static void _msg_write_userauth_hostbased_message(struct msg_buffer_s *mb, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u, struct ssh_string_s *signature)
{

    msg_write_byte(mb, SSH_MSG_USERAUTH_REQUEST);
    msg_write_ssh_string(mb, 's', (void *) s_u);
    msg_write_ssh_string(mb, 's', (void *) service);
    msg_write_ssh_string(mb, 'c', (void *) "hostbased");
    msg_write_pkalgo(mb, pkey->algo);
    msg_write_pkey(mb, pkey, PK_DATA_FORMAT_SSH_STRING);
    msg_write_ssh_string(mb, 's', (void *) c_h);
    msg_write_ssh_string(mb, 's', (void *) c_u);
    msg_write_pksignature(mb, pkey->algo, signature);

}

static unsigned int _write_userauth_hostbased_message(struct msg_buffer_s *mb, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u, struct ssh_string_s *signature)
{
    _msg_write_userauth_hostbased_message(mb, s_u, service, pkey, c_h, c_u, signature);
    return mb->pos;
}

void msg_write_userauth_hostbased_request(struct msg_buffer_s *mb, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u)
{
    _msg_write_userauth_hostbased_message(mb, s_u, service, pkey, c_h, c_u, NULL);
}

int send_userauth_hostbased_message(struct ssh_connection_s *connection, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u, struct ssh_string_s *signature, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=_write_userauth_hostbased_message(&mb, s_u, service, pkey, c_h, c_u, signature) + 64;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_USERAUTH_REQUEST;
    set_msg_buffer_payload(&mb, payload);
    payload->len=_write_userauth_hostbased_message(&mb, s_u, service, pkey, c_h, c_u, signature);
    return write_ssh_packet(connection, payload, seq);
}

/*
    send a password auth request (RFC4252 8. Password Authentication Method: "password")

    - byte 	SSH_MSG_USERAUTH_REQUEST
    - string	username used to connect
    - string	service name
    - string	"password"
    - boolean	FALSE
    - string	plaintext password

*/

static void _msg_write_userauth_password_message(struct msg_buffer_s *mb, char *user, char *pw, struct ssh_string_s *service)
{
    msg_write_byte(mb, SSH_MSG_USERAUTH_REQUEST);
    msg_write_ssh_string(mb, 'c', (void *) user);
    msg_write_ssh_string(mb, 's', (void *) service);
    msg_write_ssh_string(mb, 'c', (void *) "password");
    msg_write_byte(mb, 0);
    msg_write_ssh_string(mb, 'c', (void *) pw);
}

static unsigned int _write_userauth_password_message(struct msg_buffer_s *mb, char *user, char *pw, struct ssh_string_s *service)
{
    _msg_write_userauth_password_message(mb, user, pw, service);
    return mb->pos;
}

int send_userauth_password_message(struct ssh_connection_s *connection, char *user, char *pw, struct ssh_string_s *service, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=_write_userauth_password_message(&mb, user, pw, service) + 64;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    if (user==NULL || pw==NULL) {

	logoutput("send_userauth_password_message: user and/or pw NULL");
	return -1;

    }

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_USERAUTH_REQUEST;
    set_msg_buffer_payload(&mb, payload);
    payload->len=_write_userauth_password_message(&mb, user, pw, service);
    return write_ssh_packet(connection, payload, seq);
}

int send_userauth_request_reply(struct ssh_connection_s *connection, unsigned int methods, unsigned char success, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;

    if (success==1 && methods==0) {
	char buffer[sizeof(struct ssh_payload_s) + 8];
	struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

	init_ssh_payload(payload, 64);

	/* auth success */

	payload->type=SSH_MSG_USERAUTH_SUCCESS;
	set_msg_buffer_payload(&mb, payload);
	msg_write_byte(&mb, SSH_MSG_USERAUTH_SUCCESS);

	payload->len=mb.pos;
	return write_ssh_packet(connection, payload, seq);

    } else {
	unsigned int len=write_required_auth_methods(NULL, 0, methods);
	char namelist[len];
	char buffer[sizeof(struct ssh_payload_s) + len + 8];
	struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

	init_ssh_payload(payload, len + 64);

	payload->type=SSH_MSG_USERAUTH_FAILURE;
	set_msg_buffer_payload(&mb, payload);

	memset(namelist, 0, len);
	len=write_required_auth_methods(namelist, len, methods);

	msg_write_byte(&mb, SSH_MSG_USERAUTH_FAILURE);
	msg_write_ssh_string(&mb, 'c', namelist);
	msg_write_byte(&mb, success);

	payload->len=mb.pos;
	return write_ssh_packet(connection, payload, seq);

    }

    return -1;

}

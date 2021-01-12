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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#include "log.h"
#include "main.h"
#include "misc.h"

#include "datatypes.h"
#include "ssh-utils.h"
#include "pk-types.h"
#include "pk-keys.h"
#include "pk-readwrite-public.h"

#define SSH_CERT_TYPE_USER			1
#define SSH_CERT_TYPE_HOST			2

/* format of openssh.com certificates:
    (see: PROTOCOL.certkeys)

    string			name
    string			nonce
    union
				rsakey
				dsskey
				ed25519key
    uint64			serial
    uint32			type
    string			key_id
    string			valid_principals
    uint64			valid_after
    uint64			valid_before
    string			critical_options
    string			extensions
    string			reserved
    string			signature_key
    string			signature
*/

void msg_read_certificate_openssh_com(struct msg_buffer_s *mb, struct openssh_cert_s *cert)
{
    struct ssh_string_s tmp=SSH_STRING_INIT;

    /* the name is already read */

    msg_read_ssh_string(mb, &cert->nonce);
    msg_read_pkey(mb, &cert->key, PK_DATA_FORMAT_PARAM); /* just read the params */
    msg_read_uint64(mb, &cert->serial);
    msg_read_uint32(mb, &cert->type);
    msg_read_ssh_string(mb, &cert->key_id);
    msg_read_ssh_string(mb, &cert->valid_principals);
    msg_read_uint64(mb, &cert->valid_after);
    msg_read_uint64(mb, &cert->valid_before);
    msg_read_ssh_string(mb, &cert->critical_options);
    msg_read_ssh_string(mb, &cert->extensions);
    msg_read_ssh_string(mb, &cert->reserved);

    msg_read_ssh_string(mb, &tmp);
    cert->signature.pkalgo=read_pkalgo(tmp.ptr, tmp.len, NULL);
    msg_read_ssh_string(mb, &cert->signature.data);

}

int read_cert_openssh_com(struct openssh_cert_s *cert, struct ssh_string_s *data)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;

    set_msg_buffer(&mb, data->ptr, data->len);
    msg_read_certificate_openssh_com(&mb, cert);
    return (mb.error>0) ? -1 : 0;
}

void init_ssh_cert_openssh_com(struct openssh_cert_s *cert, struct ssh_pkalgo_s *pkalgo)
{

    memset(cert, 0, sizeof(struct openssh_cert_s));

    cert->pkalgo=pkalgo;
    init_ssh_string(&cert->nonce);
    init_ssh_key(&cert->key, 0, pkalgo); 	/* initialize the key according the schema */
    cert->serial=0;
    cert->type=0;
    init_ssh_string(&cert->key_id);
    init_ssh_string(&cert->valid_principals);
    cert->valid_after=0;
    cert->valid_before=0;
    init_ssh_string(&cert->critical_options);
    init_ssh_string(&cert->extensions);
    init_ssh_string(&cert->reserved);

    cert->signature.pkalgo=NULL;		/* not necesarly the same as key of certificate */
    init_ssh_string(&cert->signature.data);

}

static unsigned int msg_write_cert_openssh_com(struct msg_buffer_s *mb, struct openssh_cert_s *cert)
{
    msg_write_pkalgo(mb, cert->pkalgo);
    msg_write_ssh_string(mb, 's', (void *) &cert->nonce);
    msg_write_pkey(mb, &cert->key, PK_DATA_FORMAT_SSH_STRING);
    msg_store_uint64(mb, cert->serial);
    msg_store_uint32(mb, cert->type);
    msg_write_ssh_string(mb, 's', (void *) &cert->key_id);
    msg_write_ssh_string(mb, 's', (void *) &cert->valid_principals);
    msg_store_uint64(mb, cert->valid_after);
    msg_store_uint64(mb, cert->valid_before);
    msg_write_ssh_string(mb, 's', (void *) &cert->critical_options);
    msg_write_ssh_string(mb, 's', (void *) &cert->extensions);
    msg_write_ssh_string(mb, 's', (void *) &cert->reserved);
    msg_write_ssh_string(mb, 'c', (void *) &cert->pkalgo->name);
    return mb->pos;

}

static int check_signature_cert_openssh_com(struct openssh_cert_s *cert)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len = msg_write_cert_openssh_com(&mb, cert) + 64; /* extra bytes for sure */
    char buffer[len];
    struct ssh_key_s signkey;
    struct ssh_pkalgo_s *algo=cert->signature.pkalgo;

    init_ssh_key(&signkey, 0, algo);

    set_msg_buffer(&mb, buffer, len);
    len=msg_write_cert_openssh_com(&mb, cert);

    return (* signkey.verify)(&signkey, buffer, len, &cert->signature.data, algo->hash);

}

int check_cert_openssh_com(struct openssh_cert_s *cert, const char *what)
{
    struct timespec current;
    int result=-1;

    /* perform checks the certificate is valid by looking at:
	- valid before and after
	- type
	- signature
    */

    if (strcmp(what, "host")==0) {

	if (cert->type != SSH_CERT_TYPE_HOST) {

	    logoutput("check_cert_openssh_com: expecting host certificate but type does not match");
	    goto out;

	}

    } else if (strcmp(what, "user")==0) {

	if (cert->type != SSH_CERT_TYPE_USER) {

	    logoutput("check_cert_openssh_com: expecting user certificate but type does not match");
	    goto out;

	}

    }

    get_current_time(&current);

    if ((current.tv_sec < cert->valid_after) || (current.tv_sec >= cert->valid_before)) {

	logoutput("check_cert_openssh_com: certificate is not valid (anymore)");

    }

    result=check_signature_cert_openssh_com(cert);

    out:
    return result;

}

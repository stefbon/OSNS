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
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "main.h"

#include "misc.h"

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-data.h"

#include "ssh-receive.h"
#include "ssh-send.h"
#include "keyx.h"

static struct keyex_ops_s ecdh_ops;

static unsigned int populate_keyex_ecdh(struct ssh_connection_s *c, struct keyex_ops_s *ops, struct algo_list_s *alist, unsigned int start)
{

    if (alist) {

	alist[start].type=SSH_ALGO_TYPE_KEX;
	alist[start].order=SSH_ALGO_ORDER_MEDIUM;
	alist[start].sshname="curve25519-sha256@libssh.org";
	alist[start].libname="curve25519-sha256@libssh.org";
	alist[start].ptr=(void *) ops;

    }

    start++;
    return start;

}

#if HAVE_LIBGCRYPT

#include <gcrypt.h>

#define ECC_CURVE25519_LENGTH			32
#define GCRY_ECC_CURVE25519			1

#define _ED25519_BASEPOINT			"0x0900000000000000000000000000000000000000000000000000000000000000"

static unsigned char convert_hex2dec(unsigned char *hex)
{
    unsigned char result=0;

    if ((hex[0] >= 'A' && hex[0] <= 'F') || (hex[0] >= 'f' && hex[0] <= 'f')) {

	result=(unsigned char) (toupper(hex[0]) - 'A') + 10;

    } else if (hex[0] >= '0' && hex[0] <= '9') {

	result=(unsigned char) (hex[0] - '0');

    }

    return result;

}

static void convert_hex2char(unsigned char **p_hex, unsigned char *uchar)
{
    unsigned char tmp=0;
    unsigned char *hex=*p_hex;

    tmp=convert_hex2dec(&hex[0]);
    *uchar = tmp << 4;
    tmp=convert_hex2dec(&hex[1]);
    *uchar += tmp;
    *p_hex+=2;
}

static int ecc_mul_point(int algo, unsigned char *buffer, struct ssh_string_s *skey, struct ssh_string_s *pkey)
{
    gpg_error_t err=0;
    unsigned int len=gcry_ecc_get_algo_keylen(algo);
    int result=-1;
    char point[len];

    if (len==0) {

	logoutput_warning("ecc_mul_point: algo %i not reckognized", algo);
	return -1;

    } else if (len != skey->len || (pkey && len != pkey->len)) {

	logoutput_warning("ecc_mul_point: key length of public and/or private not equal to %i", len);
	return -1;

    }

    /* what to do when pkey is empty? then it should take the basepoint {9} but how to do this? */

    memset(point, 0, len);

    if (pkey) {

	memcpy(point, pkey->ptr, len);

    } else {
	unsigned int tmp=strlen(_ED25519_BASEPOINT) - 2; /* length minus 0x prefix */
	unsigned int size=((tmp+1) / 2);
	unsigned char hexbuff[tmp];
	unsigned char charbuff[size + 1];
	unsigned char *pos=hexbuff;

	memcpy(hexbuff, (char *)(_ED25519_BASEPOINT + 2), tmp); /* skip the prefix */
	memset(charbuff, '\0', size+1);

	for (unsigned int i=0; i<size; i++) convert_hex2char(&pos, &charbuff[i]);

	if (size==len) {

	    memcpy(point, charbuff, size);

	} else {
	    unsigned int cnt=(len>size) ? size : len;

	    memcpy(point, charbuff + size, cnt);

	}

    }

    err=gcry_ecc_mul_point(algo, buffer, skey->ptr, point);

    if (err) {

	logoutput("ecc_mul_point: gcry_ecc_mul_point error %s/%s", gcry_strsource(err), gcry_strerror(err));

    } else {

	result=0; /* no error */

    }

    return result;

}


    /* create a secret key like:
    - create a random 32 bytes 
    - do the following adjustments:
	buffer[0]	&=	248
	buffer[31]	&=	127
	buffer[31]	|=	64
    - read this buffer to create a public key by:
	gcry_ecc_mul_point(GCRY_ECC_CURVE25519, out, buffer, NULL);
	(using the last parameter as NULL will make it use the basepoint, and that's the desired behaviour)

	after receiving the public key of the server, calculate the scalar multiplication
	of the remote public key and the local private key
	the result is when written to buffers reversed to Little Endian
    */


static int ecdh_create_local_key(struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    unsigned int len=gcry_ecc_get_algo_keylen(GCRY_ECC_CURVE25519);
    int result=-1;

    if (len>0) {
	unsigned char buffer[len];
	struct ssh_string_s *skey_c = &ecdh->skey_c;

	gcry_create_nonce(buffer, (size_t) len);
	buffer[0]	&= 248;
	buffer[31]	&= 127;
	buffer[31]	|= 64;

	if (create_ssh_string(&skey_c, len, (char *) buffer, SSH_STRING_FLAG_ALLOC)) {

	    logoutput("ecdh_create_local_key: created client private key (len=%i) using curve 25519", len);
	    result=0;

	} else {

	    logoutput("ecdh_create_local_key: failed to create client private key");

	}

    }

    return result;
}

static void ecdh_msg_write_local_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    struct ssh_string_s *skey_c=&ecdh->skey_c;
    unsigned int len=gcry_ecc_get_algo_keylen(GCRY_ECC_CURVE25519);

    if (len>0) {
	char buffer[len];
	int result=ecc_mul_point(GCRY_ECC_CURVE25519, (unsigned char *) buffer, skey_c, NULL); /* not providing the mpoint so the basepoint is used */

	if (result==0) {
	    struct ssh_string_s pkey_c=SSH_STRING_SET(len, buffer); /* make it a string */

	    msg_write_ssh_string(mb, 's', (void *) &pkey_c);

	} else {

	    logoutput("ecdh_msg_write_local_key: unknown error writing pkey");

	}

    }

}

static void ecdh_msg_read_remote_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    struct ssh_string_s *pkey_s=&ecdh->pkey_s;

    msg_read_ssh_string(mb, pkey_s);
    logoutput("ecdh_msg_read_remote_key: len %i bytes", pkey_s->len);
}

static void ecdh_msg_write_remote_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    struct ssh_string_s *pkey_s=&ecdh->pkey_s;

    msg_write_ssh_string(mb, 's', (void *) pkey_s);
    logoutput("ecdh_msg_write_remote_key: len %i bytes", pkey_s->len);
}

static int ecdh_calc_sharedkey(struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    struct ssh_string_s *pkey_s=&ecdh->pkey_s; /* server public key */
    struct ssh_string_s *skey_c=&ecdh->skey_c; /* client private key */
    struct ssh_string_s *sharedkey=&ecdh->sharedkey;
    unsigned int len=gcry_ecc_get_algo_keylen(GCRY_ECC_CURVE25519);
    int result=-1;

    logoutput("ecdh_calc_sharedkey");

    /* check both have the same lengths */

    if (pkey_s->len==len && skey_c->len==len) {
	unsigned char buffer[len];

	/* shared key is multiplaction of the server public key and the client private key */

	if (ecc_mul_point(GCRY_ECC_CURVE25519, buffer, skey_c, pkey_s)==0) {

	    if (create_ssh_string(&sharedkey, len, (char *) buffer, SSH_STRING_FLAG_ALLOC)) {

		logoutput("ecdh_calc_sharedkey: created shared key using curve 25519 (%i bytes)", len);
		result=0;

	    } else {

		logoutput("ecdh_calc_sharedkey: error allocating %i bytes for string", len);

	    }

	} else {

	    logoutput("ecdh_calc_sharedkey: failed to create shared key");

	}

    }

    return result;

}

static unsigned char reverse_single_byte(unsigned char byte)
{
    unsigned char result=0;
    unsigned char keep=byte;

    for (unsigned int i=0; i<8; i++) {

	unsigned char a=byte & (1 << i);

	if (a>0) result |= (1 << (7-i));

    }

    logoutput("reverse_single_byte: %i to %i", keep, result);
    return result;

}

static void ecdh_msg_write_sharedkey(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    struct ssh_string_s *sharedkey=&ecdh->sharedkey;
    unsigned int len=sharedkey->len;
    char buffer[len];
    struct ssh_mpint_s mpint=SSH_MPINT_INIT;
    unsigned int error=0;
    gpg_error_t err=0;

    /* reverse the buffer to network byte order: create a temporary buffer for that */

    // for (unsigned int i=0; i<len; i++) {

	//buffer[i]=sharedkey->ptr[len-i];
	//buffer[i]=reverse_single_byte(buffer[i]);

    //}
    for (unsigned int i=0; i<len; i++) buffer[i]=sharedkey->ptr[i];

    if (read_ssh_mpint(&mpint, buffer, len, SSH_MPINT_FORMAT_USC, &error)>=0) {

	msg_write_ssh_mpint(mb, &mpint);
	free_ssh_mpint(&mpint);

    } else {

	logoutput("ecdh_msg_write_sharedkey: error reading %u bytes", len);

    }

}

#else

static int ecdh_create_local_key(struct ssh_keyex_s *k)
{
    return -1;
}
static void ecdh_msg_write_local_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
}
static void ecdh_msg_read_remote_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
}
static void ecdh_msg_write_remote_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
}
static int ecdh_calc_sharedkey(struct ssh_keyex_s *k)
{
    return -1;
}
static void ecdh_msg_write_sharedkey(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
}

#endif

static void ecdh_free_keyex(struct ssh_keyex_s *k)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;
    struct ssh_string_s *tmp=&ecdh->skey_c;

    free_ssh_string(&tmp);
    tmp=&ecdh->pkey_s;
    free_ssh_string(&tmp);
    tmp=&ecdh->sharedkey;
    free_ssh_string(&tmp);

}

static int ecdh_init_keyex(struct ssh_keyex_s *k, char *name)
{
    struct ssh_ecdh_s *ecdh=&k->method.ecdh;

    if (strcmp(name, "curve25519-sha256@libssh.org")!=0) {

	logoutput("ecdh_init_keyex: %s not supported", name);
	return -1;

    }

    strcpy(k->digestname, "sha256");
    init_ssh_string(&ecdh->pkey_s);
    init_ssh_string(&ecdh->skey_c);
    init_ssh_string(&ecdh->sharedkey);
    return 0;

}

static struct keyex_ops_s ecdh_ops = {
    .name				=	"ecdh",
    .populate				=	populate_keyex_ecdh,
    .init				=	ecdh_init_keyex,
    .create_local_key			=	ecdh_create_local_key,
    .msg_write_local_key		=	ecdh_msg_write_local_key,
    .msg_read_remote_key		=	ecdh_msg_read_remote_key,
    .msg_write_remote_key		=	ecdh_msg_write_remote_key,
    .calc_sharedkey			=	ecdh_calc_sharedkey,
    .msg_write_sharedkey		=	ecdh_msg_write_sharedkey,
    .free				=	ecdh_free_keyex,
};

void init_keyex_ecdh()
{
    add_keyex_ops(&ecdh_ops);
}

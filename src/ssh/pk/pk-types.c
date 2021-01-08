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

#include "logging.h"
#include "main.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "pk-types.h"

static struct ssh_pkalgo_s available_algos[] = {

    {
	.flags				=	SSH_PKALGO_FLAG_DEFAULT | SSH_PKALGO_FLAG_PKA,
        .scheme				=	SSH_PKALGO_SCHEME_RSA,
        .id				=	SSH_PKALGO_ID_RSA,
        .name				=	"ssh-rsa",
        .libname			=	"rsa",
        .sshname			=	"ssh-rsa",
        .hash				=	"sha1",
    },

    {
	.flags				=	SSH_PKALGO_FLAG_DEFAULT | SSH_PKALGO_FLAG_PKA,
        .scheme				=	SSH_PKALGO_SCHEME_DSS,
        .id				=	SSH_PKALGO_ID_DSS,
        .name				=	"ssh-dss",
        .libname			=	"dsa",
        .sshname			=	"ssh-dss",
        .hash				=	"sha1",
    },

    {
	.flags				=	SSH_PKALGO_FLAG_DEFAULT | SSH_PKALGO_FLAG_PKA,
        .scheme				=	SSH_PKALGO_SCHEME_ECC,
        .id				=	SSH_PKALGO_ID_ED25519,
        .name				=	"ssh-ed25519",
        .libname			=	"ed25519",
        .sshname			=	"ssh-ed25519",
        .hash				=	"sha512",
    },

    {
	.flags				=	SSH_PKALGO_FLAG_PKA,
        .scheme				=	SSH_PKALGO_SCHEME_RSA,
        .id				=	SSH_PKALGO_ID_RSA_SHA2_256,
        .name 				=	"rsa-sha2-256",
        .libname			=	"rsa-sha2-256",
        .sshname			=	"ssh-rsa",
        .hash 				=	"sha256",
    },

    {
	.flags				=	SSH_PKALGO_FLAG_PKA | SSH_PKALGO_FLAG_PREFERRED,
        .scheme				=	SSH_PKALGO_SCHEME_RSA,
        .id				=	SSH_PKALGO_ID_RSA_SHA2_512,
        .name				=	"rsa-sha2-512",
        .libname			=	"rsa-sha2-512",
        .sshname			=	"ssh-rsa",
        .hash				=	"sha512",
    },

    {
	.flags				=	SSH_PKALGO_FLAG_PKC | SSH_PKALGO_FLAG_OPENSSH_COM_CERTIFICATE,
	.scheme				=	SSH_PKALGO_SCHEME_RSA,
        .id				=	SSH_PKALGO_ID_RSA_CERT_V01_OPENSSH_COM,
        .name				=	"ssh-rsa-cert-v01@openssh.com",
        .libname			=	"rsa",
        .sshname			=	"ssh-rsa-cert-v01@openssh.com",
        .hash				=	NULL,			/* hash comes from pka in certificate at runtime */
    },

    {
	.flags				=	SSH_PKALGO_FLAG_PKC | SSH_PKALGO_FLAG_OPENSSH_COM_CERTIFICATE,
	.scheme				=	SSH_PKALGO_SCHEME_DSS,
        .id				=	SSH_PKALGO_ID_DSS_CERT_V01_OPENSSH_COM,
        .name				=	"ssh-dss-cert-v01@openssh.com",
        .libname			=	"dsa",
        .sshname			=	"ssh-dss-cert-v01@openssh.com",
        .hash				=	NULL,			/* hash comes from pka in certificate at runtime */
    },

    {
	.flags				=	SSH_PKALGO_FLAG_PKC | SSH_PKALGO_FLAG_OPENSSH_COM_CERTIFICATE,
	.scheme				=	SSH_PKALGO_SCHEME_ECC,
        .id				=	SSH_PKALGO_ID_ED25519_CERT_V01_OPENSSH_COM,
        .name				=	"ssh-ed25519-cert-v01@openssh.com",
        .libname			=	"ed25519",
        .sshname			=	"ssh-ed25519-cert-v01@openssh.com",
        .hash				=	NULL,			/* hash comes from pka in certificate at runtime */
    },

    {
	.flags				=	0,
        .scheme				=	0,
        .id				=	0,
        .name				=	NULL,
        .libname			=	NULL,
        .sshname			=	NULL,
        .hash				=	NULL},
};

void copy_pkalgo(struct ssh_pkalgo_s *a, struct ssh_pkalgo_s *b)
{
    /* do not copy the system flag */
    a->flags				=	b->flags;
    a->scheme				=	b->scheme;
    a->id				=	b->id;
    a->name				=	b->name;
    a->libname				=	b->libname;
    a->sshname				=	b->sshname;
    a->hash				=	b->hash;
}

void set_pkoptions(struct ssh_pkoptions_s *options, struct ssh_pkalgo_s *pkalgo, unsigned int o)
{

    if (pkalgo->scheme==SSH_PKALGO_SCHEME_RSA) {

	o &= ( SSH_PKALGO_OPTION_RSA_BITS_1024 | SSH_PKALGO_OPTION_RSA_BITS_2048);
	if (o>0) options->options |= o;

    } else if (pkalgo->scheme==SSH_PKALGO_SCHEME_DSS) {

	o &= ( SSH_PKALGO_OPTION_DSS_BITS_1024 | SSH_PKALGO_OPTION_DSS_BITS_2048);
	if (o>0) options->options |= o;

    } else if (pkalgo->scheme==SSH_PKALGO_SCHEME_ECC) {

	o = 0; /* no extra options for ECC for now 20180711 SB */
	if (o>0) options->options |= o;

    } else {

	logoutput("set_pkoptions: setting option for %i/%s not supported", pkalgo->id, pkalgo->name);

    }

}

struct ssh_pkalgo_s *get_pkalgo(char *name, unsigned int len, int *index)
{
    unsigned int i=0;
    struct ssh_pkalgo_s *pkalgo=NULL;

    while (available_algos[i].id>0) {

	logoutput("get_pkalgo: test %s", available_algos[i].name);

	if (strlen(available_algos[i].name)==len && strncmp(available_algos[i].name, name, len)==0) {

	    pkalgo=&available_algos[i];
	    if (index) *index=(int) i;
	    break;

	}

	i++;

    }

    return pkalgo;

}

struct ssh_pkalgo_s *get_pkalgo_string(struct ssh_string_s *s, int *index)
{
    return get_pkalgo(s->ptr, s->len, index);
}

struct ssh_pkalgo_s *get_pkalgo_byid(unsigned int id, int *index)
{
    unsigned int i=0;
    struct ssh_pkalgo_s *pkalgo=NULL;

    while (available_algos[i].id>0) {

	if (available_algos[i].id==id) {

	    pkalgo=&available_algos[i];
	    if (index) *index=(int) i;
	    break;

	}

	i++;

    }

    return pkalgo;

}

struct ssh_pkalgo_s *get_next_pkalgo(struct ssh_pkalgo_s *algo, int *index)
{

    if (algo==NULL) {

	algo=&available_algos[0];

    } else {

	algo=(struct ssh_pkalgo_s *)((char *) algo + sizeof(struct ssh_pkalgo_s));
	if (algo->id==0) algo=NULL;

    }

    return algo;
}

int get_index_pkalgo(struct ssh_pkalgo_s *algo)
{
    int i=-1;

    algo=get_pkalgo_byid(algo->id, NULL);
    if (algo==NULL) goto out;

    if ((char *) algo >= (char *) available_algos) {

	i = ((char *) algo - (char *) available_algos) / sizeof(struct ssh_pkalgo_s);
	if (i>=0 && i < (sizeof(available_algos) / sizeof(struct ssh_pkalgo_s)) && available_algos[i].id>0) return i;

    }

    out:
    return -1;
}

unsigned int write_pkalgo(char *buffer, struct ssh_pkalgo_s *pkalgo)
{
    unsigned int len=strlen(pkalgo->name);

    if (buffer) {

	store_uint32(buffer, len);
	memcpy(buffer + 4, pkalgo->name, len);

    }

    return (len + 4);

}

void msg_write_pkalgo(struct msg_buffer_s *mb, struct ssh_pkalgo_s *pkalgo)
{
    (* mb->write_ssh_string)(mb, 'c', (void *) pkalgo->name);
}

struct ssh_pkalgo_s *read_pkalgo(char *buffer, unsigned int size, int *read)
{
    if (read) *read=0;

    if (size>4) {
	unsigned int len=get_uint32(buffer);

	if (read) *read+=4;

	if (len + 4 <= size) {

	    if (read) *read+=len;
	    return get_pkalgo(buffer + 4, len, NULL);

	}

    }

    return NULL;
}

struct ssh_pkalgo_s *read_pkalgo_string(struct ssh_string_s *name, int *read)
{
    struct ssh_pkalgo_s *pkalgo=NULL;

    if (read) *read=0;
    pkalgo=get_pkalgo(name->ptr, name->len, NULL);
    if (read) *read=get_ssh_string_length(name, SSH_STRING_FLAG_HEADER | SSH_STRING_FLAG_DATA);
    return pkalgo;
}

void msg_write_pksignature(struct msg_buffer_s *mb, struct ssh_pkalgo_s *pkalgo, struct ssh_string_s *signature)
{
    unsigned int pos=0;

    if (signature==NULL || signature->ptr==NULL) return;
    pos=(* mb->start_ssh_string)(mb);
    (* mb->write_ssh_string)(mb, 'c', (void *) pkalgo->name);
    (* mb->write_ssh_string)(mb, 's', (void *) signature);
    (* mb->complete_ssh_string)(mb, pos);
}

struct ssh_pkalgo_s *msg_read_pksignature(struct msg_buffer_s *mb1, struct ssh_string_s *algo, struct ssh_string_s *signature)
{
    struct ssh_string_s tmp1=SSH_STRING_INIT;
    struct ssh_pkalgo_s *pkalgo=NULL;

    msg_read_ssh_string(mb1, &tmp1);

    if (tmp1.len>8) {
	struct msg_buffer_s mb2=INIT_SSH_MSG_BUFFER;
	struct ssh_string_s tmp2=SSH_STRING_INIT;

	set_msg_buffer(&mb2, tmp1.ptr, tmp1.len);
	if (algo==NULL) algo=&tmp2;

	msg_read_ssh_string(&mb2, algo);
	msg_read_ssh_string(&mb2, signature);

	logoutput("msg_read_pksignature: found algo %.*s", algo->len, algo->ptr);
	pkalgo=get_pkalgo(algo->ptr, algo->len, NULL);

    } else {

	logoutput("msg_read_pksignature: buffer too smal (%i)", tmp1.len);

    }

    return pkalgo;

}

const char *get_hashname_sign(struct ssh_pkalgo_s *pkalgo)
{
    return pkalgo->hash;
}

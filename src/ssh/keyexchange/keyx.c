/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "ssh-connections.h"
#include "ssh-data.h"
#include "ssh-hash.h"
#include "ssh-utils.h"
#include "ssh-keyexchange.h"
#include "ssh-send.h"
#include "ssh-receive.h"
#include "options.h"

extern struct fs_options_s fs_options;
static struct list_header_s list_keyex_ops=INIT_LIST_HEADER;

static unsigned int populate_keyex_dhnone(struct ssh_connection_s *c, struct keyex_ops_s *ops, struct algo_list_s *alist, unsigned int start)
{

    if (alist) {

	alist[start].type=SSH_ALGO_TYPE_KEX;
	alist[start].order=SSH_ALGO_ORDER_MEDIUM;
	alist[start].sshname="none";
	alist[start].libname="none";
	alist[start].ptr=(void *) ops;

    }

    start++;
    return start;

}

static int dhnone_create_local_key(struct ssh_keyex_s *k)
{
    return 0;
}

static void dhnone_msg_write_local_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    struct ssh_mpint_s tmp=SSH_MPINT_INIT;

    if (create_ssh_mpint(&tmp)==0) {

	msg_write_ssh_mpint(mb, &tmp);
	free_ssh_mpint(&tmp);

    }

}

static void dhnone_msg_read_remote_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    struct ssh_mpint_s tmp=SSH_MPINT_INIT;
    msg_read_ssh_mpint(mb, &tmp, NULL);
}

static void dhnone_msg_write_remote_key(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    dhnone_msg_write_local_key(mb, k); /* just does nothing */
}

static int dhnone_calc_sharedkey(struct ssh_keyex_s *k)
{
    return 0; /* no key to be calculated */
}

static void dhnone_msg_write_sharedkey(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
}

static void dhnone_free_keyex(struct ssh_keyex_s *k)
{
}

static int dhnone_init_keyex(struct ssh_keyex_s *k, char *name)
{
    logoutput("dhnone_init_keyex");
    return 0;
}

static struct keyex_ops_s dhnone_ops = {
    .name				=	"none",
    .populate				=	populate_keyex_dhnone,
    .init				=	dhnone_init_keyex,
    .create_local_key			=	dhnone_create_local_key,
    .msg_write_local_key		=	dhnone_msg_write_local_key,
    .msg_read_remote_key		=	dhnone_msg_read_remote_key,
    .msg_write_remote_key		=	dhnone_msg_write_remote_key,
    .calc_sharedkey			=	dhnone_calc_sharedkey,
    .msg_write_sharedkey		=	dhnone_msg_write_sharedkey,
    .free				=	dhnone_free_keyex,
};

void add_keyex_ops(struct keyex_ops_s *ops)
{
    init_list_element(&ops->list, NULL);
    add_list_element_last(&list_keyex_ops, &ops->list);
}

/* test the public host key file is present for algo on standard locations using standard naming
    is there a way to determine the client has a certain host key? */

static int lookup_hostkey_pk_list_server(struct pk_list_s *pkeys, struct ssh_pkalgo_s *algo)
{
    struct list_element_s *list=NULL;

    if (algo->libname==NULL) return -1;

    list=get_list_head(&pkeys->host_list_header, 0);
    while (list) {

	if (algo->flags & SSH_PKALGO_FLAG_PKA) {
	    struct pk_identity_s *id=(struct pk_identity_s *)((char *)list - offsetof(struct pk_identity_s, list));
	    unsigned int len=strlen("ssh_host_.key.pub") + strlen(algo->libname) + 1;
	    char buffer[len];

	    if (snprintf(buffer, len, "ssh_host_%s.key.pub", algo->libname)>0) {

		if (strcmp(buffer, id->pk.openssh_local.file)==0) break;

	    }

	} else if (algo->flags & SSH_PKALGO_FLAG_PKC) {
	    struct pk_identity_s *id=(struct pk_identity_s *)((char *)list - offsetof(struct pk_identity_s, list));
	    unsigned int len=strlen("ssh_host_.key-cert.pub") + strlen(algo->libname) + 1;
	    char buffer[len];

	    if (snprintf(buffer, len, "ssh_host_%s.key-cert.pub", algo->libname)>0) {

		if (strcmp(buffer, id->pk.openssh_local.file)==0) break;

	    }

	}

	list=get_next_element(list);
    }

    return (list) ? 0 : -1;
}

static int get_support_publickey_system_config(struct ssh_session_s *session, char *name)
{
    unsigned int len=strlen("option:ssh.crypto.pubkey.") + strlen(name) + 1;
    char what[len];
    int result=0;

    if (snprintf(what, len, "option:ssh.crypto.pubkey.%s", name)>0) {
	struct ctx_option_s option;

	init_ctx_option(&option, _CTX_OPTION_TYPE_INT);

	if ((* session->context.signal_ssh2ctx)(session, what, &option)>=0) {

	    if (ctx_option_uint(&option)) {

		result=ctx_option_get_uint(&option);

	    }

	}

    }

    return result;
}

static unsigned int build_hostkey_list(struct ssh_connection_s *c, struct algo_list_s *alist, unsigned int start)
{
    struct ssh_session_s *session=get_ssh_connection_session(c);
    struct ssh_pkalgo_s *algo=NULL;
    struct pk_list_s pkeys;
    int result=-1;

    if (session->flags & SSH_SESSION_FLAG_SERVER) {

	init_list_public_keys(NULL, &pkeys);
	result=populate_list_public_keys(&pkeys, PK_IDENTITY_SOURCE_OPENSSH_LOCAL, "host");

    }

    /* walk every pkalgo */

    algo=get_next_pkalgo(algo, NULL);

    while (algo) {

	if (algo->flags & SSH_PKALGO_FLAG_PKC) {

	    /* skip certificates if they are not supported/used here */

	    if ((fs_options.ssh.flags & _OPTIONS_SSH_FLAG_SUPPORT_CERTIFICATES)==0) goto next1;

	} else if (algo->flags & SSH_PKALGO_FLAG_PKA) {
	    int support=get_support_publickey_system_config(session, algo->sshname);

	    /* skip algo's when disabled in config */

	    if (support==-1) {

		/* explicit disabled (0=unknown, 1=enabled) */
		goto next1;

	    }

	}

	if (session->flags & SSH_SESSION_FLAG_SERVER) {

	    /* test the public host key is present on the system */

	    if (lookup_hostkey_pk_list_server(&pkeys, algo)==-1) goto next1;

	}

	if (alist) {

	    alist[start].type=SSH_ALGO_TYPE_HOSTKEY;
	    alist[start].order=(algo->flags & SSH_PKALGO_FLAG_PREFERRED) ? SSH_ALGO_ORDER_HIGH : SSH_ALGO_ORDER_MEDIUM; /**/
	    alist[start].sshname=(char *) algo->name;
	    alist[start].libname=(char *) algo->libname;
	    alist[start].ptr=NULL;

	}

	start++;
	next1:
	algo=get_next_pkalgo(algo, NULL);

    }

    return start;

}

/* get a list of supported key exchange algo's like diffie-hellman */

static unsigned int build_keyex_list(struct ssh_connection_s *c, struct algo_list_s *alist, unsigned int start)
{
    struct ssh_session_s *session=get_ssh_connection_session(c);
    struct list_element_s *list=NULL;

    if ((c->setup.flags & SSH_SETUP_FLAG_TRANSPORT)==0) {

	if (fs_options.ssh.flags & _OPTIONS_SSH_FLAG_SUPPORT_EXT_INFO) {

	    if (alist) {

		alist[start].type=SSH_ALGO_TYPE_KEX;
		alist[start].order=SSH_ALGO_ORDER_MEDIUM; /* RFC 8380 2.1 Signaling of Extension Negotiation in SSH_MSG_KEXINIT */

		if (session->flags & SSH_SESSION_FLAG_SERVER) {

		    alist[start].sshname="ext-info-s";
		    alist[start].libname="ext-info-s";

		} else {

		    alist[start].sshname="ext-info-c";
		    alist[start].libname="ext-info-c";

		}

		alist[start].ptr=NULL;

	    }

	    start++;

	}

    }

    /* add the keyex methods already registered */

    list=get_list_head(&list_keyex_ops, 0);

    while (list) {

	struct keyex_ops_s *ops=((struct keyex_ops_s *)((char *)list - offsetof(struct keyex_ops_s, list)));

	start=(* ops->populate)(c, ops, alist, start);
	list=get_next_element(list);

    }

    return start;

}

static void init_algo_list(struct algo_list_s *algo, unsigned int count)
{
    memset(algo, 0, sizeof(struct algo_list_s) * count);

    for (unsigned int i=0; i<count; i++) {

	algo[i].type=-1;
	algo[i].order=0;
	algo[i].sshname=NULL;
	algo[i].libname=NULL;
	algo[i].ptr=NULL;

    }

}

static unsigned int build_algo_list(struct ssh_connection_s *c, struct algo_list_s *algos)
{
    unsigned int start=0;

    start=build_cipher_list_s2c(c, algos, start);
    start=build_hmac_list_s2c(c, algos, start);
    start=build_compress_list_s2c(c, algos, start);
    start=build_cipher_list_c2s(c, algos, start);
    start=build_hmac_list_c2s(c, algos, start);
    start=build_compress_list_c2s(c, algos, start);
    start=build_hostkey_list(c, algos, start);
    start=build_keyex_list(c, algos, start);
    /* ignore the languages */

    return start;
}

static int set_keyex_method(struct ssh_keyex_s *k, struct algo_list_s *algo_kex, struct algo_list_s *algo_pk)
{
    struct keyex_ops_s *ops=(struct keyex_ops_s *) algo_kex->ptr;
    struct ssh_pkalgo_s *pkalgo=NULL;
    char *name=NULL;
    int result=-1;

    /* check the server hostkey algo is supported  (must be since it's a result of the algo negotiation) */

    name=algo_pk->sshname;
    pkalgo = get_pkalgo((char *)name, strlen(name), NULL);

    if (pkalgo) {

	/* hostkey is like ssh-rsa, ssh-dsa */

	logoutput("set_keyex_method: hostkey pkalgo %s supported", name);
	k->algo=pkalgo;

    } else {

	logoutput("set_keyex_method: hostkey method %s not supported", name);
	goto out;

    }

    /* initialize the keyex calls like generate client k and compute shared key */

    k->ops=ops;

    if ((* ops->init)(k, algo_kex->sshname)==0) {

	logoutput("set_keyex_method: set method %s", algo_kex->sshname);
	result=0;

    } else {

	logoutput("set_keyex_method: failed to set to method %s", algo_kex->sshname);

    }

    out:
    return result;

}

int key_exchange(struct ssh_connection_s *connection)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_receive_s *receive=&connection->receive;
    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
    unsigned int count=build_algo_list(connection, NULL) + 1;
    struct algo_list_s algos[count];
    unsigned int error=EIO;
    int result=-1;
    struct ssh_keyex_s keyex;
    struct timespec expire;

    logoutput("key_exchange (algos count=%i)", count);

    memset(&keyex, 0, sizeof(struct ssh_keyex_s));
    keyex.algo=NULL;
    keyex.flags=(session->flags & SSH_SESSION_FLAG_SERVER) ? SSH_KEYEX_FLAG_SERVER : 0;

    /* fill the algo list with supported algorithms for:
	- encryption (aes...)
	- digest (hmac...)
	- publickey (ssh-rsa...)
	- compression (zlib...)
	- key exchange (dh...)
    */

    init_algo_list(algos, count);
    count=build_algo_list(connection, algos);
    kex->algos=algos;

    /* start the exchange of algo's
	output is stored in session->setup.phase.transport.kex.chosen[SSH_ALGO_TYPE_...] 
	which are the indices of the algo array  */

    if (start_algo_exchange(connection)==-1) {

	logoutput("key_exchange: algo exchange failed");
	goto out;

    }

    if (check_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXINIT_C2S | SSH_KEX_FLAG_KEXINIT_S2C)<1) {

	logoutput("key_exchange: error: keyexchange failed");
	goto out;

    }

    if (kex->chosen[SSH_ALGO_TYPE_HOSTKEY]==-1 || kex->chosen[SSH_ALGO_TYPE_KEX]==-1) {

	logoutput("key_exchange: no hostkey algo found");
	goto out;

    }

    if (set_keyex_method(&keyex, &algos[kex->chosen[SSH_ALGO_TYPE_KEX]], &algos[kex->chosen[SSH_ALGO_TYPE_HOSTKEY]])==0) {

	logoutput("key_exchange: keyex method set to %s using hostkey type %s", algos[kex->chosen[SSH_ALGO_TYPE_KEX]].sshname, algos[kex->chosen[SSH_ALGO_TYPE_HOSTKEY]].sshname);

    } else {

	goto out;

    }

    if (start_diffiehellman(connection, &keyex)==-1) {

	logoutput("key_exchange: keyex method failed");
	goto out;

    }

    logoutput("key_exchange: send newkeys");

    /* send newkeys to server */

    if (send_newkeys_message(connection)>0) {

	logoutput("key_exchange: newkeys send");

    } else {

	logoutput("key_exchange: failed to send newkeys");
	goto out;

    }

    /* wait for all the flags to be set */

    if (wait_ssh_connection_setup_change(connection, "transport", SSH_TRANSPORT_TYPE_KEX, 0, NULL, NULL)==0) {
	int index_compr=kex->chosen[SSH_ALGO_TYPE_COMPRESS_S2C];
	int index_cipher=kex->chosen[SSH_ALGO_TYPE_CIPHER_S2C];
	int index_hmac=kex->chosen[SSH_ALGO_TYPE_HMAC_S2C];
	struct algo_list_s *algo_compr=&algos[index_compr];
	struct algo_list_s *algo_cipher=&algos[index_cipher];
	struct algo_list_s *algo_hmac=(index_hmac>=0) ? &algos[index_hmac] : NULL;

	/* reset cipher, hmac and compression to the one aggreed in kexinit
	    new keys are already computed */

	reset_decompress(connection, algo_compr);
	reset_decrypt(connection, algo_cipher, algo_hmac);
	result=0;
	set_ssh_receive_behaviour(connection, "kexfinish");

    } else {

	logoutput("key_exchange: error witing for newkeys from server");
	error=EPROTO;

    }

    out:
    return result;

}

void init_keyex_once()
{

    init_list_header(&list_keyex_ops, SIMPLE_LIST_TYPE_EMPTY, 0);

    // init_keyex_ecdh();
    init_keyex_dh();
    add_keyex_ops(&dhnone_ops);
}

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
#include <sys/syscall.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#define LOGGING
#include "log.h"

#include "main.h"
#include "misc.h"
#include "options.h"

#include "workspace.h"
#include "workspace-interface.h"
#include "fuse.h"

#include "network.h"
#include "sftp.h"
#include "workspace-fs.h"

#define _SFTP_NETWORK_NAME			"SFTP_Network"
#define _SFTP_HOME_MAP				"home"
#define _SFTP_DEFAULT_SERVICE			"ssh-channel:sftp:home"

extern struct fs_options_s fs_options;

/* callback for the ssh backend to get options from context */

static int signal_context_ssh(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    struct service_context_s *context=get_service_context(interface);

    logoutput_info("signal_context_ssh: what %s", what);

    if (strncmp(what, "option:ssh.crypto.cipher.", 25)==0) {
	unsigned int pos=25;

	option->type=_CTX_OPTION_TYPE_INT;

	if (strcmp(&what[pos], "none")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_NONE - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "3des-cbc")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_3DES_CBC - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "aes128-cbc")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_AES128_CBC - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "aes128-ctr")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_AES128_CTR - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "aes192-cbc")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_AES192_CBC - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "aes192-ctr")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_AES192_CTR - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "aes256-cbc")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_AES256_CBC - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "aes192-ctr")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_AES256_CTR - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "chacha20-poly1305@openssh.com")==0) {

	    option->value.integer=(fs_options.ssh.cipher & (1 << (_OPTIONS_SSH_CIPHER_CHACHA20_POLY1305_OPENSSH_COM - 1))) ? 1 : -1;

	} else {

	    option->value.integer=0;

	}

    } else if (strncmp(what, "option:ssh.crypto.hmac.", 23)==0) {
	unsigned int pos=23;

	option->type=_CTX_OPTION_TYPE_INT;

	if (strcmp(&what[pos], "none")==0) {

	    option->value.integer=(fs_options.ssh.hmac & (1 << (_OPTIONS_SSH_HMAC_NONE - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "hmac-sha1")==0) {

	    option->value.integer=(fs_options.ssh.hmac & (1 << (_OPTIONS_SSH_HMAC_SHA1 - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "hmac-sha256")==0) {

	    option->value.integer=(fs_options.ssh.hmac & (1 << (_OPTIONS_SSH_HMAC_SHA256 - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "hmac-md5")==0) {

	    option->value.integer=(fs_options.ssh.hmac & (1 << (_OPTIONS_SSH_HMAC_MD5 - 1))) ? 1 : -1;

	} else {

	    option->value.integer=0;

	}

    } else if (strncmp(what, "option:ssh.crypto.compression.", 30)==0) {
	unsigned int pos=30;

	option->type=_CTX_OPTION_TYPE_INT;

	if (strcmp(&what[pos], "none")==0) {

	    option->value.integer=(fs_options.ssh.compression & (1 << (_OPTIONS_SSH_COMPRESS_NONE - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "zlib")==0) {

	    option->value.integer=(fs_options.ssh.compression & (1 << (_OPTIONS_SSH_COMPRESS_ZLIB - 1))) ? 1 : -1;

	} else {

	    option->value.integer=0;

	}

    } else if (strncmp(what, "option:ssh.crypto.keyx.", 23)==0) {
	unsigned int pos=23;

	option->type=_CTX_OPTION_TYPE_INT;

	if (strcmp(&what[pos], "dh")==0) {

	    option->value.integer=(fs_options.ssh.keyx & (1 << (_OPTIONS_SSH_KEYX_DH - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "ecdh")==0) {

	    option->value.integer=(fs_options.ssh.keyx & (1 << (_OPTIONS_SSH_KEYX_ECDH - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "none")==0) {

	    option->value.integer=(fs_options.ssh.keyx & (1 << (_OPTIONS_SSH_KEYX_NONE - 1))) ? 1 : -1;

	} else {

	    option->value.integer=0;

	}

    } else if (strncmp(what, "option:ssh.crypto.pubkey.", 25)==0) {
	unsigned int pos=25;

	option->type=_CTX_OPTION_TYPE_INT;

	if (strcmp(&what[pos], "ssh-rsa")==0) {

	    option->value.integer=(fs_options.ssh.pubkey & (1 << (_OPTIONS_SSH_PUBKEY_RSA - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "ssh-dss")==0) {

	    option->value.integer=(fs_options.ssh.pubkey & (1 << (_OPTIONS_SSH_PUBKEY_DSS - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "ssh-ed25519")==0) {

	    option->value.integer=(fs_options.ssh.pubkey & (1 << (_OPTIONS_SSH_PUBKEY_ED25519 - 1))) ? 1 : -1;

	} else {

	    option->value.integer=0;

	}

    } else if (strncmp(what, "option:ssh.crypto.certificate.", 30)==0) {
	unsigned int pos=30;

	option->type=_CTX_OPTION_TYPE_INT;

	if (strcmp(&what[pos], "ssh-rsa-cert-v01@openssh.com")==0) {

	    option->value.integer=(fs_options.ssh.certificate & (1 << (_OPTIONS_SSH_CERTIFICATE_RSA_CERT_V01_OPENSSH_COM - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "ssh-dss-cert-v01@openssh.com")==0) {

	    option->value.integer=(fs_options.ssh.certificate & (1 << (_OPTIONS_SSH_CERTIFICATE_RSA_CERT_V01_OPENSSH_COM - 1))) ? 1 : -1;

	} else if (strcmp(&what[pos], "ssh-ed25519-cert-v01@openssh.com")==0) {

	    option->value.integer=(fs_options.ssh.certificate & (1 << (_OPTIONS_SSH_CERTIFICATE_ED25519_CERT_V01_OPENSSH_COM - 1))) ? 1 : -1;

	} else {

	    option->value.integer=0;

	}

    } else if (strcmp(what, "option:ssh.timeout_init")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.timeout_init;

    } else if (strcmp(what, "option:ssh.timeout_init")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.timeout_session;

    } else if (strcmp(what, "option:ssh.timeout_exec")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.timeout_exec;

    } else if (strcmp(what, "option:ssh.timeout_userauth")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.timeout_userauth;

    } else if (strcmp(what, "io:shared-signal")==0) {
	struct service_context_s *root_context=get_root_context(context);

	/* get the "root" shared signal from fuse */

	option->type=_CTX_OPTION_TYPE_PVOID;
	option->value.ptr=(void *) get_fusesocket_signal(&root_context->interface);

    } else if (strcmp(what, "command:disconnected:")==0) {

	/* TODO: */

    } else {

	return -1;

    }

    return (unsigned int) option->type;
}

struct service_context_s *create_ssh_server_service_context(struct service_context_s *networkctx, struct interface_list_s *ilist, uint32_t unique)
{
    struct service_context_s *context=NULL;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(networkctx);

    /* no parent yet */

    context=create_service_context(workspace, NULL, ilist, SERVICE_CTX_TYPE_BROWSE, NULL);

    if (context) {

	context->service.browse.type=SERVICE_BROWSE_TYPE_NETHOST;
	context->service.browse.unique=unique;
	context->flags |= (networkctx->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND);
	context->interface.signal_context=signal_context_ssh;

	set_context_filesystem_workspace(context);
	set_name_service_context(context);

	logoutput("create_ssh_server_service_context: created context %s (unique %i)", context->name, unique);

    }

    return context;

}

static int compare_starting_substring(char *name, unsigned int len, const char *start)
{
    unsigned int lens=strlen(start);

    if (len>=lens && strncmp(name, start, lens)==0) return 0;
    return -1;
}

unsigned int get_remote_services_ssh_server(struct service_context_s *context, void *ptr)
{
    struct context_interface_s *interface=&context->interface;
    struct ctx_option_s option;
    int size=0;
    unsigned int count=0;

    logoutput_info("get_remote_services_ssh_server");

    /* get a list with services supported by the ssh server */

    init_ctx_option(&option, _CTX_OPTION_TYPE_BUFFER);
    size=(* interface->signal_interface)(interface, "info:enumservices:", &option);

    /* result is comma seperated list like:
	ssh-channel:sftp:home:,ssh-channel:sftp:public: */

    if (ctx_option_error(&option)) {

	if (ctx_option_buffer(&option)==0) {

	    logoutput("get_remote_services_ssh_server: unknown error");

	} else {
	    unsigned int len=0;
	    char *data=ctx_option_get_buffer(&option, &len);
	    char tmp[len+1];

	    memset(tmp, 0, len+1);
	    memcpy(tmp, data, len);

	    logoutput("get_remote_services_ssh_server: error %s", tmp);

	}

    } else if (ctx_option_buffer(&option)) {
	unsigned int tmp=0;
	char *data=ctx_option_get_buffer(&option, &tmp);
	char list[tmp + 2];
	char *sep=NULL;
	char *item=NULL;

	logoutput("get_remote_services_ssh_server: received %s", data);

	/* make sure the comma seperated list ends with one like:

	    firstname,secondname,thirdname,fourthname,
	    so walking and searching through this list is much easier */

	memset(list, 0, tmp+2);
	memcpy(list, data, tmp);
	list[tmp]=',';
	list[tmp+1]='\0';

	item=list;

	findservice:

	sep=memchr(item, ',', strlen(item));

	if (sep) {
	    unsigned int len=0;

	    *sep='\0';
	    len=strlen(item);
	    logoutput("get_remote_supported_services: found %s", item);

	    if (compare_starting_substring(item, len, "ssh-channel:sftp:")==0) {

		count += add_ssh_channel_sftp(context, item, len, &item[strlen("ssh-channel:sftp:")], ptr);

	    }

	    *sep=',';
	    item=sep+1;
	    goto findservice;

	}

    } else {

	logoutput("get_remote_services_ssh_server: nothing received");

    }

    ctx_option_free(&option);
    return count;

}

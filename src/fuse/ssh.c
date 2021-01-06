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
#include "logging.h"

#include "main.h"
#include "misc.h"
#include "options.h"

#include "workspace.h"
#include "workspace-interface.h"
#include "fuse.h"

#include "network.h"
#include "sftp.h"

#define _SFTP_NETWORK_NAME			"SFTP_Network"
#define _SFTP_HOME_MAP				"home"

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

    } else if (strcmp(what, "option:ssh.init_timeout")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.init_timeout;

    } else if (strcmp(what, "option:ssh.session_timeout")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.session_timeout;

    } else if (strcmp(what, "option:ssh.exec_timeout")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.exec_timeout;

    } else if (strcmp(what, "option:ssh.userauth_timeout")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=(unsigned int) fs_options.ssh.userauth_timeout;

    } else if (strcmp(what, "io:shared-mutex")==0) {
	struct service_context_s *root_context=get_root_context(context);

	/* get the "root" shared mutex from fuse */

	option->type=_CTX_OPTION_TYPE_PVOID;
	option->value.ptr=(void *) get_fuse_pthread_mutex(root_context->interface.buffer);

    } else if (strcmp(what, "io:shared-cond")==0) {
	struct service_context_s *root_context=get_root_context(context);

	/* get the "root" shared cond from fuse */

	option->type=_CTX_OPTION_TYPE_PVOID;
	option->value.ptr=(void *) get_fuse_pthread_cond(root_context->interface.buffer);

    } else if (strcmp(what, "command:disconnected:")==0) {

	/* TODO: */

    } else {

	return -1;

    }

    return (unsigned int) option->type;
}

/* connect to ssh server
    at inode of virtual map */

static struct service_context_s *connect_ssh_server(struct workspace_mount_s *workspace, struct host_address_s *host, struct service_address_s *service, struct inode_s *inode, struct interface_list_s *ilist)
{
    struct service_context_s *context=NULL;
    struct context_interface_s *interface=NULL;
    int fd=-1;

    context=create_service_context(workspace, get_workspace_context(workspace), ilist, SERVICE_CTX_TYPE_CONNECTION, NULL);
    if (context==NULL) return NULL;
    interface=&context->interface;

    logoutput("connect_ssh_server: connect");

    fd=(* interface->connect)(workspace->user->pwd.pw_uid, interface, host, service);
    if (fd<0) {

	logoutput("connect_ssh_server: failed to connect");
	goto error;

    }

    logoutput("connect_ssh_server: start");

    if ((* interface->start)(interface, fd, NULL)==0) {

	logoutput("connect_ssh_server: started ssh connection");
	return context;

    }

    error:

    remove_list_element(&context->list);
    free_service_context(context);
    return NULL;

}

static void get_remote_supported_services(struct service_context_s *context, struct inode_s *inode, struct interface_list_s *ailist)
{
    struct context_interface_s *interface=&context->interface;
    struct ctx_option_s option;
    int size=0;
    unsigned int count=0;

    logoutput_info("get_remote_supported_services");

    /* get a list with services supported by the ssh server */

    init_ctx_option(&option, _CTX_OPTION_TYPE_BUFFER);
    size=(* interface->signal_interface)(interface, "info:enumservices:", &option);

    /* result is comma seperated list like:
	ssh-channel:sftp:home:,ssh-channel:sftp:public: */

    if (size>=0 && option.type==_CTX_OPTION_TYPE_BUFFER && option.value.buffer.ptr && option.value.buffer.len>0) {
	char list[option.value.buffer.len + 2];
	char *sep=NULL;
	char *service=NULL;
	unsigned int left=strlen(list);

	memcpy(list, option.value.buffer.ptr, option.value.buffer.len);
	list[option.value.buffer.len]=',';
	list[option.value.buffer.len+1]='\0';
	service=list;

	findservice:

	sep=memchr(service, ',', left);

	if (sep) {

	    *sep='\0';
	    logoutput("get_remote_supported_services: found %s", service);

	    if (strncmp(service, "ssh-channel:", 12)==0) {
		unsigned int pos=12;

		if (strncmp(&service[pos], "sftp:", 5)==0) {

		    pos+=5;
		    add_shared_map_sftp(context, inode, &service[pos], ailist);
		    count++;

		}

	    }

	    *sep=',';
	    left-=(sep - service);
	    service=sep+1;
	    goto findservice;

	}

    }

    trydefault:

    if (count==0) {

	logoutput("get_remote_supported_services: no services found, try the default (home)");
	add_shared_map_sftp(context, inode, _SFTP_HOME_MAP, ailist);

    }

    (* option.free)(&option);

}

static int test_buffer_ip(char *hostname)
{
    if (check_family_ip_address(hostname, "ipv4")==1) return 0;
    if (check_family_ip_address(hostname, "ipv6")==1) return 0;
    return -1;
}

static struct entry_s *install_virtualnetwork_map(struct service_context_s *context, struct entry_s *parent, char *name, const char *what)
{
    struct entry_s *entry=NULL;
    unsigned int error=0;
    struct directory_s *directory01=get_directory(parent->inode);
    struct simple_lock_s wlock01;

    if (wlock_directory(directory01, &wlock01)==0) {
	struct name_s xname;
	struct directory_s *directory02=NULL;
	struct simple_lock_s wlock02;
	struct inode_s *inode=NULL;

	xname.name=name;
	xname.len=strlen(name);
	xname.index=0;
	calculate_nameindex(&xname);

	entry=find_entry_batch(directory01, &xname, &error);

	/* only install if not exists */

	if (entry==NULL) {

	    error=0;
	    entry=create_network_map_entry(context, directory01, &xname, &error);

	    if (entry) {

		logoutput_info("install_virtualnetwork_map: created map %s", name);

	    } else {

		logoutput_warning("install_virtualnetwork_map: unable to create map %s", name);
		unlock_directory(directory01, &wlock01);
		goto out;

	    }

	} else {

	    logoutput_info("install_virtualnetwork_map: map %s already exists", name);
	    unlock_directory(directory01, &wlock01);
	    goto out;

	}

	inode=entry->inode;
	directory02=get_directory(inode);

	if (wlock_directory(directory02, &wlock02)==0) {

	    inode->nlookup=1;

	    if (strcmp(what, "network")==0) {

		if (fs_options.network.network_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_desktopentry_file("/etc/fs-workspace/desktopentry.network", entry, context->workspace);

	    } else if (strcmp(what, "domain")==0) {

		if (fs_options.network.domain_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_desktopentry_file("/etc/fs-workspace/desktopentry.netgroup", entry, context->workspace);

	    } else if (strcmp(what, "server")==0) {
		struct inode_link_s link;

		/* attach the server context to the inode representing the ssh server */

		link.type=INODE_LINK_TYPE_CONTEXT;
		link.link.ptr=(void *) context;
		set_inode_link_directory(inode, &link);

		if (fs_options.network.server_icon & (_OPTIONS_NETWORK_ICON_SHOW | _OPTIONS_NETWORK_ICON_OVERRULE))
		    create_desktopentry_file("/etc/fs-workspace/desktopentry.netserver", entry, context->workspace);

	    }

	    unlock_directory(directory02, &wlock02);

	}

	unlock_directory(directory01, &wlock01);

    }

    out:
    return entry;

}

/*
    connect to ssh server and use the sftp subsystem to browse
    - connect to the server and the home directory
    - create a "server" entry with the name of the address
    - rename this "server" entry to a more human readable name (unique??)
    - add this entry to the SSH network map
    note the directory of parent is already excl locked
*/

int install_ssh_server_context(struct workspace_mount_s *workspace, struct entry_s *parent, struct host_address_s *host, struct service_address_s *service, unsigned int *error)
{
    struct service_context_s *context=get_workspace_context(workspace);
    struct context_interface_s *interface=NULL;
    int result=-1;
    struct host_address_s tmp;
    struct interface_list_s *ilist=NULL;
    struct ctx_option_s option;
    char *domain=NULL;
    unsigned int count=build_interface_ops_list(&context->interface, NULL, 0);
    struct interface_list_s ailist[count + 1];

    logoutput("install_ssh_server_context");

    /* build the list with available interface ops
	important here are of course the ops to setup a ssh server context and a sftp server context (=ssh channel) */

    for (unsigned int i=0; i<count+1; i++) {

	ailist[i].type=-1;
	ailist[i].name=NULL;
	ailist[i].ops=NULL;

    }

    count=build_interface_ops_list(&context->interface, ailist, 0);

    /* look for the interface ops for a ssh session */

    ilist=get_interface_ops(ailist, count+1, _INTERFACE_TYPE_SSH_SESSION);

    if (ilist==NULL) {

	*error=EINVAL;
	return -1;

    }

    context=connect_ssh_server(workspace, host, service, NULL, ilist);
    if (! context) return -1;
    interface=&context->interface;

    if ((fs_options.sftp.flags & _OPTIONS_SFTP_FLAG_SHOW_NETWORKNAME) && fs_options.sftp.network_name) {

	struct entry_s *entry=install_virtualnetwork_map(context, parent, fs_options.sftp.network_name, "network");
	if (entry) parent=entry;

    }

    init_host_address(&tmp);

    /* get full name including domain */

    init_ctx_option(&option, _CTX_OPTION_TYPE_BUFFER);

    if ((* interface->signal_interface)(interface, "info:servername:", &option)>=0) {

	if (option.type==_CTX_OPTION_TYPE_BUFFER && option.value.buffer.ptr && option.value.buffer.len>0) {

	    if ((option.flags & _CTX_OPTION_FLAG_ERROR)==0) {

		if (option.value.buffer.len < sizeof(tmp.hostname)) {

		    memcpy(tmp.hostname, option.value.buffer.ptr, option.value.buffer.len);

		} else {

		    /* hostname too long */

		    logoutput("install_ssh_server_context: servername from server too long (%i) ignoring", option.value.buffer.len);

		}

	    } else {

		logoutput("install_ssh_server_context: received error %.*s", option.value.buffer.len, option.value.buffer.ptr);

	    }

	}

    }

    (* option.free)(&option);

    if (strlen(tmp.hostname)==0) {

	init_ctx_option(&option, _CTX_OPTION_TYPE_BUFFER);

	if ((* interface->signal_interface)(interface, "info:fqdn:", &option)>=0) {

	    if (option.type==_CTX_OPTION_TYPE_BUFFER && option.value.buffer.ptr && option.value.buffer.len>0) {

		if ((option.flags & _CTX_OPTION_FLAG_ERROR)==0) {

		    if (option.value.buffer.len < sizeof(tmp.hostname)) {

			memcpy(tmp.hostname, option.value.buffer.ptr, option.value.buffer.len);

		    } else {

			/* hostname too long */

			logoutput("install_ssh_server_context: hostname from server too long (%i) ignoring", option.value.buffer.len);

		    }

		}

	    } else {

		logoutput("install_ssh_server_context: received error %.*s", option.value.buffer.len, option.value.buffer.ptr);

	    }

	}

    }

    (* option.free)(&option);

    /* if still not found try hostname from parameters */

    if (strlen(tmp.hostname)==0) {

	if (host->flags & HOST_ADDRESS_FLAG_HOSTNAME) {

	    memcpy(tmp.hostname, host->hostname, sizeof(host->hostname));
	    tmp.flags = HOST_ADDRESS_FLAG_HOSTNAME;

	}

    }

    if (strlen(tmp.hostname)>0 && test_buffer_ip(tmp.hostname)==-1) {
	char *sep=NULL;

	tmp.flags |= HOST_ADDRESS_FLAG_HOSTNAME;

	/* get rid of nasty characters */

	replace_cntrl_char(tmp.hostname, strlen(tmp.hostname), REPLACE_CNTRL_FLAG_TEXT);
	skip_trailing_spaces(tmp.hostname, strlen(tmp.hostname), SKIPSPACE_FLAG_REPLACEBYZERO);

	/* look for the second name (seperated with a dot) */

	sep=strchr(tmp.hostname, '.');

	if (sep) {

	    *sep='\0';
	    domain=sep+1;
	    logoutput("install_ssh_server_context: found domain %s", domain);

	}

    }

    logoutput("install_ssh_server_context: found servername %s", tmp.hostname);

    if ((fs_options.sftp.flags & _OPTIONS_SFTP_FLAG_SHOW_DOMAINNAME) && domain) {

	struct entry_s *entry=install_virtualnetwork_map(context, parent, domain, "domain");
	if (entry) parent=entry;

    }

    logoutput("install_ssh_server_context: install server map %s", tmp.hostname);

    /* install the server map */

    if (strlen(tmp.hostname)>0) {

	struct entry_s *entry=install_virtualnetwork_map(context, parent, tmp.hostname, "server");
	if (entry) {

	    /* create sftp shared directories in server map */

	    get_remote_supported_services(context, entry->inode, ailist);
	    result=0;

	}

    }

    out:
    return result;

    error:
    return -1;

}

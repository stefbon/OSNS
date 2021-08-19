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


#include "main.h"
#include "log.h"
#include "options.h"
#include "misc.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "interface/sftp-prefix.h"
#include "network.h"
#include "sftp-fs.h"
#include "sftp.h"

#define _SFTP_NETWORK_NAME			"SFTP_Network"
#define _SFTP_HOME_MAP				"home"
#define _SFTP_DEFAULT_SERVICE			"ssh-channel:sftp:home:"

extern struct fs_options_s fs_options;

static int signal_sftp2ctx(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    struct service_context_s *context=get_service_context(interface);

    logoutput_info("signal_ctx_sftp: what %s", what);

    if (strcmp(what, "option:sftp.usermapping.user-unknown")==0) {

	option->type=_CTX_OPTION_TYPE_PCHAR;
	option->value.name=fs_options.sftp.usermapping_user_unknown;

    } else if (strcmp(what, "option:sftp.usermapping.user-nobody")==0) {

	option->type=_CTX_OPTION_TYPE_PCHAR;
	option->value.name=fs_options.sftp.usermapping_user_nobody;

    } else if (strcmp(what, "option:sftp.usermapping.type")==0) {

	if (fs_options.sftp.usermapping_type==_OPTIONS_SFTP_USERMAPPING_NONE) {

	    option->value.name="none";
	    option->type=_CTX_OPTION_TYPE_PCHAR;

	} else if (fs_options.sftp.usermapping_type==_OPTIONS_SFTP_USERMAPPING_MAP) {

	    option->value.name="map";
	    option->type=_CTX_OPTION_TYPE_PCHAR;

	} else if (fs_options.sftp.usermapping_type==_OPTIONS_SFTP_USERMAPPING_FILE) {

	    option->value.name="file";
	    option->type=_CTX_OPTION_TYPE_PCHAR;

	}

    } else if (strcmp(what, "option:sftp.usermapping.file")==0) {

	option->type=_CTX_OPTION_TYPE_PCHAR;
	option->value.name=fs_options.sftp.usermapping_file;

    } else if (strcmp(what, "option:sftp.packet.maxsize")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=fs_options.sftp.packet_maxsize;

    } else if (strcmp(what, "option:sftp:correcttime")==0) {

	option->type=_CTX_OPTION_TYPE_INT;
	option->value.integer=1;

    } else if (strcmp(what, "event:disconnect:")==0) {

	/* set the filesystem to disconnected mode */

	set_context_filesystem_sftp(context, 1);

    } else {

	return -1;

    }

    return (unsigned int) option->type;
}

/* get prefix and uri from a string like:

    /home/sbon|
    /home/public|/var/run/sftp.sock|
    /home/admin|ssh://192.168.1.6:3001|

    the first part is the prefix on the server,
    the second part is the uri to connect to (if empty the default sftp subsystem is used)
*/

static int get_service_info_prefix(char *data, unsigned int len, char **p_prefix, char **p_uri)
{
    char *pos=data;
    int left=len;
    char *sep=memchr(pos, '|', left);

    if (sep) {

	*p_prefix=pos;
	*sep='\0';
	sep++;

    } else {

	return -1;

    }

    left -= (int)(sep - data);
    if (left < 3) return 0;

    pos=sep;

    sep=memchr(pos, '|', left);
    if (sep) {

	*p_uri=pos;
	*sep='\0';

    }

    return 0;
}

static struct service_context_s *create_sftp_client_context(struct service_context_s *sshctx, struct interface_list_s *ilist)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(sshctx);
    struct service_context_s *ctx=NULL;

    logoutput("create_sftp_client_context: create sftp context for filesystem");

    ctx=create_service_context(workspace, sshctx, ilist, SERVICE_CTX_TYPE_FILESYSTEM, NULL);

    if (ctx) {

	ctx->service.filesystem.inode=NULL;
	ctx->flags |= (sshctx->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND);
	ctx->interface.signal_context=signal_sftp2ctx;

	set_context_filesystem_sftp(ctx, 0);
	set_name_service_context(ctx);

    }

    return ctx;

}

static struct service_context_s *create_start_sftp_client_context(struct service_context_s *sshctx, struct sftp_service_s *service, struct interface_list_s *ilist)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(sshctx);
    struct service_context_s *context=NULL;
    struct context_interface_s *interface=NULL;
    struct service_address_s address;
    int fd=-1;
    unsigned int error=0;
    struct simple_lock_s wlock;
    struct directory_s *directory=NULL;

    logoutput("create_start_sftp_client_context: create sftp client context %s", service->fullname);

    context=create_sftp_client_context(sshctx, ilist);
    if (context==NULL) {

	logoutput("create_start_sftp_client_context: error allocating context");
	return NULL;

    }

    memset(&address, 0, sizeof(struct service_address_s));
    address.type=_SERVICE_TYPE_SFTP_CLIENT;
    address.target.sftp.name=service->name;
    address.target.sftp.uri=service->uri;
    interface=&context->interface;

    fd=(* interface->connect)(workspace->user->pwd.pw_uid, interface, NULL, &address);

    if (fd==-1) {

	logoutput("create_start_sftp_client_context: error connecting");
	goto error;

    } else {

	logoutput("create_start_sftp_client_context: sftp client connected");

    }

    if ((* interface->start)(interface, fd, NULL)==0) {

	logoutput("create_start_sftp_client_context: sftp client started");

    } else {

	logoutput("create_start_sftp_client_context: unable to start sftp client");
	goto error;

    }

    set_sftp_interface_prefix(interface, service->name, service->prefix);
    return context;

    error:

    if (context) {

	unset_parent_service_context(sshctx, context);
	(* interface->signal_interface)(interface, "command:close:", NULL);
	(* interface->signal_interface)(interface, "command:clear:", NULL);
	free_service_context(context);
	context=NULL;

    }

    return NULL;

}

/* add a sftp shared map */

struct service_context_s *add_shared_map_sftp(struct service_context_s *context, struct sftp_service_s *service, void *ptr)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct discover_service_s *discover=(struct discover_service_s *) ptr;
    unsigned int error=0;
    struct ctx_option_s option;
    struct interface_list_s *ilist=NULL;
    struct service_context_s *sftpctx=NULL;

    ilist=get_interface_list(discover->ailist, discover->count, _INTERFACE_TYPE_SFTP);
    if (ilist==NULL) {

	logoutput_warning("add_shared_map_sftp: sftp interface not found");
	return NULL;

    }

    logoutput("add_shared_map_sftp: service %s", service->fullname);

    /* get details about this service like prefix and possibly the socket like :

	/home/public|socket://run/fileserver/sock|
	(direct-streamlocal)

	or

	/home/sbon|
	(default sftp subsystem)

	or

	/home/joe|ssh://192.168.1.8:2222|
	(direct-tcpip)

    */

    init_ctx_option(&option, _CTX_OPTION_TYPE_PCHAR);
    option.value.name=service->fullname;

    if ((* interface->signal_interface)(interface, "info:service:", &option)>=0) {

	if (ctx_option_error(&option)) {

	    if (ctx_option_buffer(&option)==0) {

		logoutput("add_shared_map_sftp: unknown error");

	    } else {
		unsigned int len=0;
		char *data=ctx_option_get_buffer(&option, &len);
		char tmp[len+1];

		memset(tmp, 0, len+1);
		memcpy(tmp, data, len);

		logoutput("add_shared_map_sftp: error %s", tmp);

	    }

	} else if (ctx_option_buffer(&option)) {
	    unsigned int len=0;
	    char *data=ctx_option_get_buffer(&option, &len);
	    char *prefix=NULL;
	    char *uri=NULL;

	    if (get_service_info_prefix(data, len, &prefix, &uri)==-1) {

		(* option.free)(&option);
		goto error;

	    }

	    service->uri=uri;
	    service->prefix=prefix;

	    sftpctx=create_start_sftp_client_context(context, service, ilist);

	    if (sftpctx) {

		logoutput("add_shared_map_sftp: sftp client context added for %s (prefix=%s, uri=%s)", service->fullname, (prefix ? prefix : "NULL"), (uri ? uri : "NULL"));

	    } else {

		logoutput("add_shared_map_sftp: failed to add sftp client context");
		ctx_option_free(&option);
		goto error;

	    }

	}

	ctx_option_free(&option);

    }

    if (sftpctx==NULL) {

	sftpctx=create_start_sftp_client_context(context, service, ilist);

	if (sftpctx) {

	    logoutput("add_shared_map_sftp: sftp client context added for %s (no prefix, no uri)", service->fullname);

	} else {

	    logoutput("add_shared_map_sftp: failed to add sftp client context");

	}

    }

    out:
    return sftpctx;

    error:
    logoutput("add_shared_map_sftp: error");
    return NULL;

}

unsigned int add_ssh_channel_sftp(struct service_context_s *context, char *fullname, unsigned int len, char *name, void *ptr)
{
    struct sftp_service_s service;
    unsigned int len2=strlen(name);
    char tmp[len2+1];
    unsigned int count=0;

    memset(tmp, 0, len2+1);
    memcpy(tmp, name, len2);

    replace_cntrl_char(tmp, len2, REPLACE_CNTRL_FLAG_TEXT);
    if (skip_heading_spaces(tmp, len2)>0) len2=strlen(tmp);
    if (skip_trailing_spaces(tmp, len2, SKIPSPACE_FLAG_REPLACEBYZERO)>0) len2=strlen(tmp);

    memset(&service, 0, sizeof(struct sftp_service_s));

    service.fullname=fullname;
    service.name=tmp;
    service.prefix=NULL;
    service.uri=NULL;

    if (add_shared_map_sftp(context, &service, ptr)) count++;
    return count;

}

unsigned int add_default_ssh_channel_sftp(struct service_context_s *context, void *ptr)
{
    return add_ssh_channel_sftp(context, _SFTP_DEFAULT_SERVICE, strlen(_SFTP_DEFAULT_SERVICE), _SFTP_HOME_MAP, ptr);
}


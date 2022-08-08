/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-resources.h"

#include "fuse/browse-fs.h"
#include "interface/ssh.h"
#include "interface/ssh-utils.h"

#include "osns_client.h"

#include "network.h"
#include "utils.h"
#include "ssh-context.h"

#include "ssh/ssh-common.h"
#include "ssh/ssh-common-client.h"
#include "sftp/common.h"

static char *get_sftp_service_ssh_server(struct context_interface_s *i, char *name, unsigned int *p_len)
{
    struct io_option_s option;

    init_io_option(&option, _IO_OPTION_TYPE_PCHAR);
    option.value.name=name;

    if ((* i->iocmd.in)(i, "info:service:", &option, i, 0)>=0) {

	if ((option.flags & _IO_OPTION_FLAG_ERROR)==0) {

	    if (option.type==_IO_OPTION_TYPE_BUFFER) {
		char *service=NULL;

		if (option.value.buffer.len==0) {

		    logoutput_debug("get_sftp_service_ssh_server: no data received");
		    if (p_len) *p_len=0;
		    return NULL;

		}

		if (p_len) *p_len=option.value.buffer.len;
		return option.value.buffer.ptr;

	    }

	} else {

	    if (option.type==_IO_OPTION_TYPE_BUFFER) {
		unsigned int len=option.value.buffer.len;
		char tmp[len+1];

		memset(tmp, 0, len+1);
		memcpy(tmp, option.value.buffer.ptr, len);

		logoutput("get_sftp_service_ssh_server: error %s", tmp);

	    } else {

		logoutput("get_sftp_service_ssh_server: unknown error");

	    }

	}

	(* option.free)(&option);

    }

    if (p_len) *p_len=0;
    return NULL;

}

/* get prefix

    /home/sbon|
    /home/public/shared|
    /home/admin|

    the first part is the prefix on the server,

*/

static int get_service_info_prefix(char *data, unsigned int len, char *prefix)
{
    char *pos=data;

    if (len>0) {
	char *sep=memchr(pos, '|', len);

	if (sep) {

	    if (sep>pos) memcpy(prefix, pos, (unsigned int)(sep - pos));
	    return (int)(sep - pos);

	}

    }

    return -1;
}

static int get_service_name(char *data, unsigned int len, char *name)
{
    int result=-1;

    if (compare_starting_substring(data, len, "ssh-channel:sftp:")==0) {
	unsigned int tmplen=strlen("ssh-channel:sftp:");
	unsigned int size=len - tmplen;

	if (name) {
	    char *sep=NULL;

	    memcpy(name, &data[tmplen], size);
	    sep=memchr(name, ':', size);
	    if (sep) *sep='\0';

	}

	result=size+1;

    }

    return result;

}

static int connect_ssh_channel_context_sftp(struct service_context_s *ctx)
{
    const char *uri="session-subsystem://sftp";
    return connect_ssh_channel_context(ctx, uri);
}

#define COMPARE_HLPR_FLAGS_FOUND		1

struct compare_ssh_channel_hlpr_s {
    struct service_context_s			*ctx_shared;
    unsigned int				flags;
};

static int compare_ssh_channel_ctx(struct service_context_s *ctx, void *ptr)
{
    int result=-1;

    if ((ctx->interface.type==_INTERFACE_TYPE_SSH_CHANNEL) && (ctx->interface.flags & _INTERFACE_FLAG_SECONDARY_1TON)) {
	struct ssh_channel_s *channel=(struct ssh_channel_s *) ctx->interface.buffer;
	struct compare_ssh_channel_hlpr_s *hlpr=(struct compare_ssh_channel_hlpr_s *) ptr;

	if ((ctx->interface.link.primary==&hlpr->ctx_shared->interface) &&
	    (channel->type==_CHANNEL_TYPE_SESSION) &&
	    (channel->target.session.type==_CHANNEL_SESSION_TYPE_SUBSYSTEM) &&
	    strcmp(channel->target.session.buffer, "sftp")==0) {

	    result=0;
	    hlpr->flags |= COMPARE_HLPR_FLAGS_FOUND;

	}

    }

    return result;

}

int create_sftp_filesystem_context(struct service_context_s *ctx_shared, struct service_context_s *ctx_browse, char *info)
{
    unsigned int len=0;
    char *service=NULL;
    int size=get_service_name(info, strlen(info), NULL);

    if ((ctx_shared==NULL) || (ctx_browse==NULL) || size<=0) return -1;
    service=get_sftp_service_ssh_server(&ctx_shared->interface, info, &len);

    if (service) {
	struct workspace_mount_s *w=get_workspace_mount_ctx(ctx_shared);
	char prefix[len+1];
	struct compare_ssh_channel_hlpr_s hlpr;
	struct service_context_s *ssh_channel_ctx=NULL;
	struct service_context_s *sftp_shared_client_ctx=NULL;
	struct service_context_s *sftp_filesystem_client_ctx=NULL;
	unsigned char action=0;
	char name[size+1];
	char tmp[len+1];

	/* TODO:
	    - create the ssh channel ctx first, use any existing to the same target, make it a shared context
	    - create a sftp client ctx, also shared (cause it maybe used by more than one)
	    - create a filesystem ctx, which makes use of the sftp client ctx
	*/

	memset(name, 0, size+1);
	size=get_service_name(info, strlen(info), name);

	/* get the prefix from the service string */

	memset(prefix, 0, len+1);

	/* copy service string to temporay buffer to manipulate the spaces/newline etc */

	memset(tmp, 0, len+1);
	memcpy(tmp, service, len);
	replace_newline_char(tmp, len);
	len=strlen(tmp);

	if (skip_heading_spaces(tmp, len)>0) len=strlen(tmp);
	if (skip_trailing_spaces(tmp, len, SKIPSPACE_FLAG_REPLACEBYZERO)>0) len=strlen(tmp);

	if (get_service_info_prefix(tmp, len, prefix)==-1) {

	    logoutput_debug("create_sftp_browse_context: unable to get prefix from %s - %s", tmp, name);
	    return -1;

	} else {

	    logoutput_debug("create_sftp_browse_context: found prefix %s from %s - %s", prefix, tmp, name);

	}

	/* SSH CHANNEL CTX */

	hlpr.ctx_shared=ctx_shared;
	hlpr.flags=0;

	ssh_channel_ctx=create_network_shared_context(w, 0, 0, NETWORK_SERVICE_TYPE_SSH, _INTERFACE_TYPE_SSH_CHANNEL, ctx_shared, compare_ssh_channel_ctx, (void *)&hlpr);
	if (ssh_channel_ctx==NULL) return -1;

	if ((hlpr.flags & COMPARE_HLPR_FLAGS_FOUND)==0) {

	    if (connect_ssh_channel_context_sftp(ssh_channel_ctx)>=0) {

		logoutput_debug("create_sftp_browse_context: channel connected and started");

	    } else {

		logoutput_debug("create_sftp_browse_context: unable to start/connect channel");
		remove_context(w, ssh_channel_ctx);
		free(ssh_channel_ctx);
		return -1;

	    }

	}

	/* SHARED SFTP CLIENT CTX
	    it's possible that is exists already since a shared sftp client can be uh... shared 
	    in that case the ssh channel must exist also since there is a 1to1 relation between
	    a ssh channel and a sftp client (using the sftp subsystem) */

	if (ssh_channel_ctx->interface.flags & _INTERFACE_FLAG_PRIMARY_1TO1) {
	    struct context_interface_s *tmp=ssh_channel_ctx->interface.link.secondary.interface;

	    if (tmp) sftp_shared_client_ctx=get_service_context(tmp);

	}

	if (sftp_shared_client_ctx==NULL) {

	    sftp_shared_client_ctx=create_network_shared_context(w, 0, NETWORK_SERVICE_TYPE_SFTP, NETWORK_SERVICE_TYPE_SSH, _INTERFACE_TYPE_SFTP_CLIENT, ssh_channel_ctx, NULL, NULL);
	    if (sftp_shared_client_ctx==NULL) return -1;

	    if (connect_sftp_client_context(sftp_shared_client_ctx, NULL)>=0) {

		logoutput_debug("create_sftp_browse_context: sftp client connected and started");

	    } else {

		logoutput_debug("create_sftp_browse_context: unable to start/connect sftp client");
		remove_context(w, sftp_shared_client_ctx);
		free(sftp_shared_client_ctx);
		return -1;

	    }

	}

	/* FILESYSTEM SFTP CLIENT CTX */

	sftp_filesystem_client_ctx=check_create_install_context(w, ctx_browse, 0, name, NETWORK_SERVICE_TYPE_SFTP, sftp_shared_client_ctx, &action);
	if (sftp_filesystem_client_ctx==NULL) return -1;

	if (connect_sftp_client_context(sftp_filesystem_client_ctx, prefix)>=0) {

	    logoutput_debug("create_sftp_browse_context: sftp client connected and started");

	} else {

	    logoutput_debug("create_sftp_browse_context: unable to start/connect sftp client");
	    remove_context(w, sftp_filesystem_client_ctx);
	    free(sftp_filesystem_client_ctx);
	    return -1;

	}

    }

    return 0;

}

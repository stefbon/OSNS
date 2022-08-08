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

static int _signal_interface2ctx(struct context_interface_s *i, const char *what, struct io_option_s *o)
{
    logoutput_debug("_signal_interface2ctx: what %s", what);
    return 0;
}

int connect_ssh_context_default(struct service_context_s *ctx, union interface_target_u *target, union interface_parameters_u *param)
{
    struct context_interface_s *i=&ctx->interface;
    int fd=-1;

    logoutput("connect_ssh_context_default: ctx type %u i flags %u", ctx->type, i->flags);

    fd=(* i->connect)(i, target, param);

    if (fd>=0) {

	if ((* i->start)(i)==0) {

	    logoutput("connect_ssh_context_default: connected and started");

	} else {

	    logoutput("connect_ssh_context_default: unable to start");
	    fd=-1;

	}

    } else {

	logoutput("connect_ssh_context_default: unable to connect");

    }

    if (fd==-1) {

	(* i->iocmd.in)(i, "command:close:", NULL, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
	(* i->iocmd.in)(i, "command:free:", NULL, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);

    }

    return fd;

}

int connect_ssh_session_context(struct service_context_s *ctx, struct host_address_s *address, struct network_port_s *port)
{
    union interface_target_u target;
    union interface_parameters_u param;

    memset(&target, 0, sizeof(union interface_target_u));
    memset(&param, 0, sizeof(union interface_parameters_u));

    target.host=address;
    param.port=port;

    return connect_ssh_context_default(ctx, &target, &param);

}

int connect_ssh_channel_context(struct service_context_s *ctx, char *uri)
{
    union interface_target_u target;
    union interface_parameters_u param;

    memset(&target, 0, sizeof(union interface_target_u));
    memset(&param, 0, sizeof(union interface_parameters_u));

    target.uri=uri;

    return connect_ssh_context_default(ctx, &target, &param);

}

int connect_sftp_client_context(struct service_context_s *ctx, char *prefix)
{
    union interface_target_u target;
    union interface_parameters_u param;
    struct sftp_target_s sftp;

    memset(&target, 0, sizeof(union interface_target_u));
    memset(&param, 0, sizeof(union interface_parameters_u));

    if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	sftp.flags=0;
	sftp.prefix=prefix;
	target.sftp=&sftp;

    }

    return connect_ssh_context_default(ctx, &target, &param);

}

static int connect_ssh_host(struct service_context_s *ctx)
{
    struct context_interface_s *i=&ctx->interface;
    struct network_resource_s nr;
    struct network_resource_s host_nr;
    int result=0;

    if (ctx->interface.flags & _INTERFACE_FLAG_CONNECT) return 0;

    logoutput_debug("connect_ssh_host: ctx type %u", ctx->type);

    /* get address and port from */

    memset(&nr, 0, sizeof(struct network_resource_s));
    nr.type=NETWORK_RESOURCE_TYPE_NETWORK_SOCKET;

    result=get_network_resource(ctx->service.shared.unique, &nr);

    if (result==0 || result==-1) {

	logoutput_debug("connect_ssh_host: socket unique %u not found", ctx->service.shared.unique);
	return -1;

    }

    memset(&host_nr, 0, sizeof(struct network_resource_s));
    host_nr.type=NETWORK_RESOURCE_TYPE_NETWORK_HOST;

    result=get_network_resource(nr.parent_unique, &host_nr);

    if (result==0 || result==-1) {

	logoutput_debug("connect_ssh_host: host unique %u not found", nr.parent_unique);
	return -1;

    }

    return connect_ssh_session_context(ctx, &host_nr.data.address, &nr.data.service.port);

}

struct ssh_context_service_s {
    struct service_context_s				*ctx;
    unsigned int					count;
    int							(* cb)(struct service_context_s *ctx, char *name, void *ptr);
    void						*ptr;
};

static void process_ssh_commalist_hlpr(char *name, void *ptr)
{
    struct ssh_context_service_s *hlpr=(struct ssh_context_service_s *) ptr;

    if (name==NULL || strlen(name)==0) return;

    if ((* hlpr->cb)(hlpr->ctx, name, hlpr->ptr)==1) {

	hlpr->count++;

    }

}

unsigned int get_remote_services_ssh_host(struct service_context_s *ctx, int (* cb)(struct service_context_s *ctx, char *name, void *ptr), void *ptr)
{
    struct context_interface_s *i=&ctx->interface;
    struct io_option_s option;
    int size=0;
    unsigned int count=0;

    if (ctx==NULL) {

	logoutput_warning("get_remote_services_ssh_host: ctx not defined");
	return -1;

    }

    logoutput_debug("get_remote_services_ssh_host: ctx type %u name %s", ctx->type, ctx->name);

    /* get a list with services supported by the ssh server
	how the ssh client does this is configured there (exec a command, global request, an extension or just the default) */

    init_io_option(&option, _IO_OPTION_TYPE_BUFFER);
    size=(* i->iocmd.in)(i, "info:enumservices:", &option, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);

    /* result is comma seperated list like:
	ssh-channel:sftp:home:,ssh-channel:sftp:public: */

    if ((option.flags & _IO_OPTION_FLAG_ERROR)==0) {

	if (option.type==_IO_OPTION_TYPE_BUFFER) {
	    struct ssh_context_service_s hlpr;
	    unsigned int len=option.value.buffer.len;
	    char tmp[len+1];

	    if (len<2) {

		logoutput_debug("get_remote_services_ssh_host: no data received");
		goto out;

	    }

	    memset(tmp, 0, len+1);
	    memcpy(tmp, option.value.buffer.ptr, len);
	    replace_newline_char(tmp, len);
	    len=strlen(tmp);

	    if (skip_heading_spaces(tmp, len)>0) len=strlen(tmp);
	    if (skip_trailing_spaces(tmp, len, SKIPSPACE_FLAG_REPLACEBYZERO)>0) len=strlen(tmp);

	    memset(&hlpr, 0, sizeof(struct ssh_context_service_s));
	    hlpr.ctx=ctx;
	    hlpr.count=0;
	    hlpr.ptr=ptr;
	    hlpr.cb=cb;

	    parse_ssh_commalist(tmp, len, process_ssh_commalist_hlpr, (void *) &hlpr);
	    logoutput_debug("get_remote_services_ssh_host: %u services parsed", hlpr.count);
	    count=hlpr.count;

	}

    } else {

	if (option.type==_IO_OPTION_TYPE_BUFFER) {
	    unsigned int len=option.value.buffer.len;
	    char tmp[len+1];

	    memset(tmp, 0, len+1);
	    memcpy(tmp, option.value.buffer.ptr, len);

	    logoutput("get_remote_services_ssh_server: error %s", tmp);

	} else {

	    logoutput("get_remote_services_ssh_server: unknown error");

	}

    }

    out:
    (* option.free)(&option);
    return count;

}

struct get_remote_service_hlpr_s {
    struct service_context_s *ctx_browse;
    struct service_context_s *ctx_shared;
};

static int get_remote_service_cb(struct service_context_s *ssh_ctx, char *name, void *ptr)
{
    int result=0;
    unsigned int len=(name ? strlen(name) : 0);

    logoutput_debug("get_remote_service_cb: name %s", name);

    if (len>0) {

	if (compare_starting_substring(name, len, "ssh-channel:sftp:")==0) {
	    struct get_remote_service_hlpr_s *hlpr=(struct get_remote_service_hlpr_s *) ptr;

	    if (create_sftp_filesystem_context(hlpr->ctx_shared, hlpr->ctx_browse, name)==0) {

		result=1;

	    }

	}

    }

    return result;
}

static void thread_connect_ssh_host(void *ptr)
{
    struct service_context_s *ctx=(struct service_context_s *) ptr;
    struct context_interface_s *i=&ctx->interface;
    struct service_context_s *ctx_shared=NULL;
    uint32_t unique=0;

    logoutput_debug("thread_connect_ssh_host: context type %u interface flags %u", ctx->type, i->flags);

    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) unique=ctx->service.browse.unique;

    if (unique>0) {
	struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);

	/* create or find the shared context for the ssh transport */

	ctx_shared=create_network_shared_context(w, unique, 0, NETWORK_SERVICE_TYPE_SSH, _INTERFACE_TYPE_SSH_SESSION, NULL, NULL, NULL);

	if (ctx_shared==NULL) {

	    logoutput_debug("thread_connect_ssh_host: unable to create shared context ... cannot continue");
	    return;

	}

    }

    if (ctx_shared) {

	if (connect_ssh_host(ctx_shared)>=0) {
	    struct get_remote_service_hlpr_s hlpr;

	    logoutput_debug("thread_connect_ssh_host: connected ... look for supported services");

	    hlpr.ctx_shared=ctx_shared;
	    hlpr.ctx_browse=ctx;

	    unsigned int count=get_remote_services_ssh_host(ctx_shared, get_remote_service_cb, (void *) &hlpr);

	}

    }

}

void start_thread_connect_ssh_host(struct service_context_s *ctx)
{
    work_workerthread(NULL, 0, thread_connect_ssh_host, (void *) ctx, NULL);
}

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
#include <sys/poll.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "users.h"

#include "smb.h"
#include "smb-signal.h"
#include "smb-eventloop.h"

extern struct context_interface_s *get_parent_interface(struct context_interface_s *i);
extern struct passwd *get_workspace_user_pwd(struct context_interface_s *i);
extern struct beventloop_s *get_workspace_eventloop(struct context_interface_s *i);
extern struct common_signal_s *get_workspace_signal(struct context_interface_s *i);

static pthread_mutex_t all_pending_lists_mutex=PTHREAD_MUTEX_INITIALIZER;

static unsigned int get_size_smb_share()
{
    return sizeof(struct smb_share_s);
}

struct smb_signal_s *get_smb_signal_ctx(struct context_interface_s *interface)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    return &share->signal;
}

void add_smb_list_pending_requests_ctx(struct context_interface_s *interface, struct list_element_s *list)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);

    pthread_mutex_lock(&all_pending_lists_mutex);
    add_list_element_last(&share->requests, list);
    pthread_mutex_unlock(&all_pending_lists_mutex);

}

void remove_smb_list_pending_requests_ctx(struct context_interface_s *interface, struct list_element_s *list)
{
    pthread_mutex_lock(&all_pending_lists_mutex);
    remove_list_element(list);
    pthread_mutex_unlock(&all_pending_lists_mutex);
}

uint32_t get_id_smb_share(struct context_interface_s *interface)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    uint32_t id=share->id;
    share->id++;
    return id;
}

struct smb2_context *get_smb2_context_smb_interface(struct context_interface_s *interface)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    return (struct smb2_context *) share->ptr;
}

static unsigned int get_username_for_smb_connection(struct context_interface_s *interface, uid_t uid, struct service_address_s *service, char *buffer, unsigned int size)
{
    unsigned int len=0;

    /*
	get the username used to make a connection to the smb server, follow the order:

	- service->smb.username: if explicit specified
	- local username belonging to uid */

    if (service->target.smb.username) {

	len=strlen(service->target.smb.username);
	if (buffer) memcpy(buffer, service->target.smb.username, len);

    } else {
	struct ssh_string_s user=SSH_STRING_INIT;
	unsigned int error=0;

	lock_local_userbase();

	if (buffer) {

	    user.ptr=buffer;
	    user.len=size;

	}

	len=get_local_user_byuid(uid, &user, &error);

	unlock_local_userbase();

    }

    logoutput("get_username_for_smb_connection: len %i user %s", len, (buffer ? buffer : "NULL"));
    return len;
}

/* SMB share callbacks */

static void _connect_smb_share_async_cb(struct smb2_context *smb2, int status, void *command_data, void *cb_data)
{
    struct context_interface_s *interface=(struct context_interface_s *) cb_data;
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct smb_signal_s *signal=get_smb_signal_ctx(interface);
    unsigned int flag=_SMB_SHARE_FLAG_CONNECTED;
    unsigned int error=0;

    if (status) {

	if (status<0) status=-status;
	logoutput_warning("_connect_smb_share_async_cb: failed to open share %i - %s - %s", status, strerror(status), smb2_get_error(smb2));
	flag = _SMB_SHARE_FLAG_ERROR;
	error=status;

    }

    logoutput("_connect_smb_share_async_cb: flag %i");

    smb_signal_lock(signal);
    share->flags |= flag;
    share->error = error;
    smb_signal_broadcast(signal);
    smb_signal_unlock(signal);

}

static int _connect_interface_smb_share(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct smb2_context *smb2=(struct smb2_context *) share->ptr;
    int result=-1;
    char *server=NULL;
    char *name=NULL;
    unsigned int len=get_username_for_smb_connection(interface, uid, service, NULL, 0);
    char username[len + 1];

    if (!(service->type==_SERVICE_TYPE_SMB_SHARE)) {

	logoutput_warning("_connect_interface_smb_share: error, connections other than smb share (service type=%i)", service->type);
	goto out;

    }

    if ((host->flags & HOST_ADDRESS_FLAG_HOSTNAME) && strlen(host->hostname)>0) {

	server=host->hostname;

    } else if ((host->flags & HOST_ADDRESS_FLAG_IP)) {

	if ((host->ip.family==IP_ADDRESS_FAMILY_IPv4) && strlen(host->ip.ip.v4)>0) {

	    server=host->ip.ip.v4;

	} else if ((host->ip.family==IP_ADDRESS_FAMILY_IPv6) && strlen(host->ip.ip.v6)>0) {

	    server=host->ip.ip.v6;

	}

    }

    if (server==NULL) {

	logoutput_warning("_connect_interface_smb_share: error, target host is invalid (no server found)");
	goto out;

    }

    name=service->target.smb.share;

    if (name==NULL || strlen(name)==0) {

	logoutput_warning("_connect_interface_smb_share: error, target smb share is invalid (no share found)");
	goto out;

    }

    memset(username, 0, len+1);
    len=get_username_for_smb_connection(interface, uid, service, username, len);

    if (strlen(username)==0) {

	logoutput_warning("_connect_interface_smb_share: username is empty");
	goto out;

    }

    logoutput("_connect_interface_smb_share: connect to smb://%s/%s as %s", server, name, username);

    if (smb2_connect_share_async(smb2, server, name, username, _connect_smb_share_async_cb, (void *) interface)==0) {

	result=smb2_get_fd(smb2);
	logoutput("_connect_interface_smb_share: connected with fd %i", result);
	// smb2_set_timeout(share->smb2, 4); /* make this configurable */

    }

    out:
    return result;
}

static int _start_interface_smb_share(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    struct beventloop_s *loop=get_workspace_eventloop(interface);
    struct bevent_s *bevent=NULL;
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct timespec timeout;

    bevent=share->bevent;

    if (bevent==NULL) {
	struct smb2_context *smb2=(struct smb2_context *) share->ptr;
	short events = smb2_which_events(smb2);

	if (events) {

	    logoutput("_start_interface_smb_share (io events=%i)", events);

	} else {

	    logoutput("_start_interface_smb_share: error no io events...");
	    goto error;

	}

	bevent=create_fd_bevent(loop, process_smb_share_event, (void *) interface);
	if (bevent==NULL) goto error;

	set_bevent_unix_fd(bevent, fd);

    // if (events & POLLIN) {

	logoutput("_start_interface_smb_share: set watch incoming data");

	set_bevent_watch(bevent, "i");
	set_bevent_watch(bevent, "u");

    // }

    // if (events & POLLOUT) {

	// logoutput("_start_interface_smb_share: set watch outgoing data");

	// set_bevent_watch(bevent, "outgoing data");

    // }

	share->bevent=bevent;

    }

    if ((bevent->flags & BEVENT_FLAG_EVENTLOOP)==0) {

	if (add_bevent_beventloop(bevent)==0) {

	    logoutput("_start_interface_smb_share: added fd %i to eventloop", fd);

	} else {

	    logoutput("_start_interface_smb_share: failed to add fd %i to eventloop", fd);
	    goto error;

	}

    }

    get_smb_request_timeout_ctx(interface, &timeout);

    logoutput("_start_interface_smb_share: wait for reply");

    if (wait_smb_share_connected(interface, &timeout)==1) {

	logoutput("_start_interface_smb_share: smb connected fd %i", fd);
	return 0;
    }

    error:

    logoutput("_start_interface_smb_share: unable to start smb share ");

    if (bevent) {

	remove_bevent(bevent);
	if (share->bevent) share->bevent=NULL;

    }

    return -1;

}

/* function called by the context to inform smb share of events/commands like umount/disconnect/free */

static int _i_signal_ctx2share(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    return (* share->context.signal_ctx2share)(share, what, option);
}

/* function called by smb share to inform the ctx of event like (unexpected) disconnect */

static int _s_signal_share2ctx(struct smb_share_s *share, const char *what, struct ctx_option_s *option)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) share - offsetof(struct context_interface_s, buffer));
    return (* interface->signal_context)(interface, what, option);
}

static int _s_signal_ctx2share(struct smb_share_s *share, const char *what, struct ctx_option_s *o)
{
    int result=-1;

    logoutput("_s_signal_ctx2share: what %s", what);

    if (strlen(what)>8 && strncmp(what, "command:", 8)==0) {
	unsigned int pos=8;

	if ((strlen(&what[pos])>=5 && strncmp(&what[pos], "free:", 5)==0) ||
	    (strlen(&what[pos])>=6 && strncmp(&what[pos], "clear:", 6)==0)) {

	    result=0;

	    if (share->ptr) {
		struct smb2_context *smb2=(struct smb2_context *) share->ptr;

		if ((share->flags & _SMB_SHARE_FLAG_CONNECTED) && (share->flags & _SMB_SHARE_FLAG_CLOSED)==0) {

		    smb2_disconnect_share(smb2);
		    share->flags |= _SMB_SHARE_FLAG_CLOSED;

		}

		if ((share->flags & _SMB_SHARE_FLAG_INIT) && (share->flags & _SMB_SHARE_FLAG_FREE)==0) {

		    smb2_destroy_context(smb2);
		    share->flags |= _SMB_SHARE_FLAG_FREE;
		    share->ptr=NULL;

		}

		result=1;

	    }


	} else if (strlen(&what[pos])>=6 && strncmp(&what[pos], "close:", 6)==0) {

	    result=0;

	    if (share->ptr) {
		struct smb2_context *smb2=(struct smb2_context *) share->ptr;

		if ((share->flags & _SMB_SHARE_FLAG_CONNECTED) && (share->flags & _SMB_SHARE_FLAG_CLOSED)==0) {

		    smb2_disconnect_share(smb2);
		    share->flags |= _SMB_SHARE_FLAG_CLOSED;

		}

		result=1;

	    }

	}

    }

    return result;

}

/* smb server callbacks (just dummys) */

static int _connect_interface_smb_server(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    return 0;
}

static int _start_interface_smb_server(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    return 0;
}

/* INTERFACE OPS */

static unsigned int _populate_smb_interface(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start)
{

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SMB_SHARE;
	ilist[start].name="smb-share";
	ilist[start].ops=(void *) ops;

    }

    start++;

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SMB_SERVER;
	ilist[start].name="smb-server";
	ilist[start].ops=(void *) ops;

    }

    start++;

    return start;
}

static unsigned int _get_interface_buffer_size(struct interface_list_s *ilist)
{

    if (ilist->type==_INTERFACE_TYPE_SMB_SHARE) {

	return get_size_smb_share();

    } else if (ilist->type==_INTERFACE_TYPE_SMB_SERVER) {

	return 0;

    }

    return 0;

}

static int _init_interface_buffer_smb_share(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct smb_share_s *share=NULL;
    struct context_interface_s *parent=NULL;
    struct interface_ops_s *ops=ilist->ops;
    struct passwd *pwd=NULL;
    struct smb2_context *smb2=NULL;

    if (interface->size < (* ops->get_buffer_size)(ilist)) {

	logoutput_warning("_init_interface_buffer_smb_share: buffer size too small (%i, required %i) cannot continue", interface->size, (* ops->get_buffer_size)(ilist));
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_smb_share: buffer already initialized");
	return 0;

    }

    parent=get_parent_interface(interface);

    if (parent==NULL) {

	logoutput_warning("_init_interface_buffer_smb_share: parent interface (= smb server) not set");

    } else if (parent->type != _INTERFACE_TYPE_SMB_SERVER) {

	logoutput_warning("_init_interface_buffer_smb_share: parent interface is not a network host/smb server (%i)", parent->type);

    }

    interface->type=_INTERFACE_TYPE_SMB_SHARE;
    share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);

    smb2=smb2_init_context();

    if (smb2) {

	share->ptr = (void *) smb2;

	interface->connect=_connect_interface_smb_share;
	interface->start=_start_interface_smb_share;
	interface->signal_interface=_i_signal_ctx2share;

	share->context.interface=interface;
	share->context.signal_share2ctx=_s_signal_share2ctx;
	share->context.signal_ctx2share=_s_signal_ctx2share;

	share->signal.signal=get_workspace_signal(interface);
	share->signal.flags=0;
	share->signal.error=0;

	init_list_header(&share->requests, SIMPLE_LIST_TYPE_EMPTY, NULL);

	share->bevent=NULL;
	share->id=0;
	share->flags=_SMB_SHARE_FLAG_INIT;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	smb2_set_opaque(smb2, (void *) interface);
	smb2_fd_event_callbacks(smb2, _smb2_change_fd_cb, _smb2_change_events_cb);

        return 0;

    }

    return -1;

}

static int _init_interface_buffer_smb_server(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct context_interface_s *parent=NULL;
    struct interface_ops_s *ops=ilist->ops;

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_smb_server: buffer already initialized");
	return 0;

    }

    interface->type=_INTERFACE_TYPE_SMB_SERVER;
    interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
    interface->connect=_connect_interface_smb_server;
    interface->start=_start_interface_smb_server;

    return 0;

}

static int _init_interface_buffer(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{

     if (ilist->type == _INTERFACE_TYPE_SMB_SHARE) {

	return _init_interface_buffer_smb_share(interface, ilist, primary);

    } else if (ilist->type == _INTERFACE_TYPE_SMB_SERVER) {

	return _init_interface_buffer_smb_server(interface, ilist, primary);

    }

    return -1;
}

static void _clear_interface_buffer(struct context_interface_s *interface)
{

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	if ((interface->flags & _INTERFACE_FLAG_BUFFER_CLEAR)==0) {

	    (* interface->signal_interface)(interface, "disconnect", NULL);
	    (* interface->signal_interface)(interface, "close", NULL);
	    (* interface->signal_interface)(interface, "free", NULL);
	    interface->flags |= _INTERFACE_FLAG_BUFFER_CLEAR;

	}

	interface->flags &= ~_INTERFACE_FLAG_BUFFER_INIT;
	reset_context_interface(interface);

    }

}

static struct interface_ops_s smb_interface_ops = {
    .name					= "SMB",
    .populate					= _populate_smb_interface,
    .get_buffer_size				= _get_interface_buffer_size,
    .init_buffer				= _init_interface_buffer,
    .clear_buffer				= _clear_interface_buffer,
};

void init_smb_share_interface()
{
    add_interface_ops(&smb_interface_ops);
}

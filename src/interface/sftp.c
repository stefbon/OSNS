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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-resources.h"

#include "ssh/ssh-common.h"
#include "sftp/common.h"
#include "sftp/recv.h"
#include "ssh/send/msg-channel.h"
#include "ssh-utils.h"
#include "sftp-attr.h"
#include "sftp/extensions.h"
#include "sftp.h"

#define SFTP_PREFIX_TYPE_HOME					0
#define SFTP_PREFIX_TYPE_ROOT					1
#define SFTP_PREFIX_TYPE_CUSTOM					2

struct sftp_client_data_s {
    unsigned int				flags;
    unsigned int				statvfs_index;
    unsigned int				fsync_index;
    int						(* compare_path)(struct context_interface_s *i, char *path, unsigned int len, unsigned int type);
    unsigned int				(* get_complete_pathlen)(struct context_interface_s *i, struct fuse_path_s *fp);
    unsigned char				type;
    unsigned int				len;
    char					path[];
};

static unsigned int get_complete_pathlen_home(struct context_interface_s *i, struct fuse_path_s *fpath)
{
    fpath->pathstart++;
    return (unsigned int)(fpath->path + fpath->len - 1 - fpath->pathstart);
}

static char *get_ssh_session_remote_home(struct context_interface_s *i)
{
    struct context_interface_s *primary=i->link.primary;
    struct ssh_channel_s *channel=NULL;
    char *home=NULL;

    if (primary->type==_INTERFACE_TYPE_SFTP_CLIENT) primary=primary->link.primary;
    if (primary->type==_INTERFACE_TYPE_SSH_CHANNEL) channel=(struct ssh_channel_s *) primary->buffer;

    if (channel) {
	struct ssh_session_s *session=channel->session;
	struct net_idmapping_s *mapping=&session->hostinfo.mapping;

	home=mapping->su.type.user.home;

    }

    return home;
}

static int compare_path_home(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    int result=-1;

    logoutput_debug("compare_path_home: path %.*s", len, path);

    switch (type) {

	case SFTP_COMPARE_PATH_PREFIX_SUBDIR:

	    if ((len>=1) && strncmp(&path[0], "/", 1)==0) {
		char *home=get_ssh_session_remote_home(i);

		/* absolute path : must be subdir of homedirectory */

		if (home) {
		    unsigned int tmp=strlen(home);

		    logoutput_debug("compare_path_home: compare with home %s", home);
		    if ((len > tmp) && (strncmp(path, home, tmp)==0) && (strncmp(&path[tmp], "/", 1)==0)) result=(int) tmp;

		}

	    } else {

		/* empty path or not starting with slash:
		    always a subdirectory of home by protocol */

		result=0;

	    }

	    break;

	default:

	    logoutput_debug("compare_path_home: type %u not reckognized", type);

    }

    return result;
}

static unsigned int get_complete_pathlen_root(struct context_interface_s *i, struct fuse_path_s *fpath)
{
    return (unsigned int)(fpath->path + fpath->len - 1 - fpath->pathstart);
}

static int compare_path_root(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    int result=-1;

    logoutput_debug("compare_path_root: path %.*s", len, path);

    switch (type) {

	case SFTP_COMPARE_PATH_PREFIX_SUBDIR:

	    if ((len>=1) && strncmp(&path[0], "/", 1)==0) result=1;
	    break;

	default:

	    logoutput_debug("compare_path_root: type %u not reckognized", type);

    }

    return result;
}

static unsigned int get_complete_pathlen_custom(struct context_interface_s *i, struct fuse_path_s *fpath)
{
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;

    logoutput_debug("get_complete_pathlen_custom: prefix %.*s path %s", data->len, data->path, fpath->pathstart);

    fpath->pathstart -= data->len;
    memcpy(fpath->pathstart, data->path, data->len);

    return (unsigned int)(fpath->path + fpath->len - 1 - fpath->pathstart);
}

static int compare_path_custom(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    int result=-1;

    logoutput_debug("compare_path_custom: path %.*s", len, path);

    switch (type) {

	case SFTP_COMPARE_PATH_PREFIX_SUBDIR:

	    if ((len > data->len) && (strncmp(path, data->path, data->len)==0) && (strncmp(&path[data->len], "/", 1)==0)) result=(int) data->len;
	    break;

	default:

	    logoutput_debug("compare_path_custom: type %u not reckognized", type);

    }

    return result;
}


static struct ssh_channel_s *get_ssh_channel_sftp_client(struct sftp_client_s *sftp)
{
    struct context_interface_s *i=(struct context_interface_s *)((char *) sftp - offsetof(struct context_interface_s, buffer));
    struct context_interface_s *primary=i->link.primary;

    return (struct ssh_channel_s *) primary->buffer;
}

static struct sftp_client_s *get_sftp_client_ssh_channel(struct ssh_channel_s *channel)
{
    struct context_interface_s *i=(struct context_interface_s *)((char *) channel - offsetof(struct context_interface_s, buffer));
    struct context_interface_s *secondary=i->link.secondary.interface;

    return (struct sftp_client_s *) secondary->buffer;
}

static int set_prefix_sftp_client(struct context_interface_s *i, char *prefix)
{
    struct sftp_client_data_s *data=NULL;
    unsigned int len=0;

    if (prefix) {
	char *home=get_ssh_session_remote_home(i);
	unsigned int tmp=((home) ? strlen(home) : 0);

	logoutput_debug("set_prefix_sftp_client: remote home %s prefix %s", home, prefix);

	len=(((strcmp(prefix, "~")==0) ||
	    ((tmp>0) && (strcmp(prefix, home)==0)) ||
	    ((tmp>0) && (strlen(prefix) == tmp + 1) && (strncmp(prefix, home, tmp)==0) && (prefix[tmp]=='/'))) ? 0 : strlen(prefix));

    } else {

	len=0;

    }

    data=malloc(sizeof(struct sftp_client_data_s) + len);

    if (data) {

	i->ptr=(void *) data;

	data->flags=0;
	data->statvfs_index=0;
	data->fsync_index=0;

	if (len==0) {

	    /* remote subdirectory is the user's homedirectory
		now by protocol when a path is send to the sftp server
		and does not start with a slash it's relative to $HOME on server */

	    data->type=SFTP_PREFIX_TYPE_HOME;
	    data->compare_path=compare_path_home;
	    data->get_complete_pathlen=get_complete_pathlen_home;
	    data->len=0;

	} else if (len==1 && strcmp(prefix, "/")==0) {

	    /* remote subdirectory is root: path's are send without modification */

	    data->type=SFTP_PREFIX_TYPE_ROOT;
	    data->compare_path=compare_path_root;
	    data->get_complete_pathlen=get_complete_pathlen_root;
	    memcpy(data->path, "/", 1);
	    data->len=1;

	} else {

	    /* remote subdirectory is custom: every path send is prepended by this */

	    data->type=SFTP_PREFIX_TYPE_CUSTOM;
	    data->compare_path=compare_path_custom;
	    data->get_complete_pathlen=get_complete_pathlen_custom;
	    memcpy(data->path, prefix, len);
	    data->len=len;

	}

    }

    return ((data) ? 0 : -1);

}

int sftp_compare_path(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    return (* data->compare_path)(i, path, len, type);
}

unsigned int sftp_get_complete_pathlen(struct context_interface_s *i, struct fuse_path_s *fpath)
{
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    return (* data->get_complete_pathlen)(i, fpath);
}

unsigned int sftp_get_required_buffer_size_p2l(struct context_interface_s *i, unsigned int len)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    return (* sftp->context.get_required_path_size_p2l)(sftp, len);
}

unsigned int sftp_get_required_buffer_size_l2p(struct context_interface_s *i, unsigned int len)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    return (* sftp->context.get_required_path_size_l2p)(sftp, len);
}

int sftp_convert_path_p2l(struct context_interface_s *i, char *buffer, unsigned int size, char *data, unsigned int len)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    return (* sftp->context.convert_path_p2l)(sftp, buffer, size, data, len);
}

int sftp_convert_path_l2p(struct context_interface_s *i, char *buffer, unsigned int size, char *data, unsigned int len)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    return (* sftp->context.convert_path_l2p)(sftp, buffer, size, data, len);
}

/* SFTP callbacks */

static int _connect_interface_shared_sftp_client(struct context_interface_s *i, union interface_target_u *target, union interface_parameters_u *param)
{
    int result=-1;

    if (i->flags & _INTERFACE_FLAG_SECONDARY_1TO1) {
	struct ssh_channel_s *channel=NULL;
	struct context_interface_s *primary=i->link.primary;

	if (primary && primary->type==_INTERFACE_TYPE_SSH_CHANNEL) channel=(struct ssh_channel_s *) primary->buffer;

	/* test the channel is suitable to use ... TODO: add direct-tcpip and direct-streamlocal channels */

	if (channel && (channel->type==_CHANNEL_TYPE_SESSION) &&
	    (channel->target.session.type==_CHANNEL_SESSION_TYPE_SUBSYSTEM) &&
	    (strcmp(channel->target.session.buffer, "sftp")==0)) {

	    result=0;

	}

    }

    return result;

}

static int _connect_interface_client_sftp_client(struct context_interface_s *interface, union interface_target_u *target, union interface_parameters_u *param)
{
    int result=-1;

    if (interface->flags & _INTERFACE_FLAG_SECONDARY_1TON) {
	struct context_interface_s *primary=interface->link.primary;

	if (primary && (primary->type == _INTERFACE_TYPE_SFTP_CLIENT)) result=0;

    }

    if (result==0) {
	char *prefix=NULL;

	/* prefix differs per client interface (used in the browse map), and is not used bt the primary sftp client */

	if (target) prefix=target->sftp->prefix;

	if (prefix) {

	    logoutput_debug("_connect_interface_sftp_client: found prefix %s", prefix);

	} else {

	    logoutput_debug("_connect_interface_sftp_client: no prefix");

	}

	result=set_prefix_sftp_client(interface, prefix);

    }

    out:
    return result;
}

static int _start_interface_shared_sftp_client(struct context_interface_s *interface)
{
    char *buffer=(* interface->get_interface_buffer)(interface);
    struct sftp_client_s *sftp=(struct sftp_client_s *) buffer;
    struct ssh_channel_s *channel=NULL;
    int result=-1;

    logoutput_debug("_start_interface_shared_sftp_client: start sftp init subsystem");

    channel=get_ssh_channel_sftp_client(sftp);

    /* switch the processing if incoming data for the channel to the sftp subsystem (=context)
	it's not required to set the cb here...it's already set in the init phase */

    switch_msg_channel_receive_data(channel, "context", NULL);
    result=start_init_sftp_client(sftp);
    if (result==-1) logoutput_warning("_start_interface_shared_sftp_client: error starting sftp subsystem");

    return result;

}

static int _start_interface_client_sftp_client(struct context_interface_s *interface)
{
    return 0;
}

/* function called by the context to inform sftp client of events */

struct select_sftp2ctx_hlpr_s {
    struct context_interface_s 				*i;
};

static int select_sftp2ctx_cb(struct context_interface_s *i, void *ptr)
{
    struct select_sftp2ctx_hlpr_s *hlpr=(struct select_sftp2ctx_hlpr_s *) ptr;

    return (((i->type==_INTERFACE_TYPE_SFTP_CLIENT) && (i->link.primary==hlpr->i)) ? 1 : 0);
}

static int _signal_ctx2sftp_secondary(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type)
{

    /* what to do with it depends on the sender */

    if (((type==INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION) || (type==INTERFACE_CTX_SIGNAL_TYPE_SSH_CHANNEL)) || (s && (s==i->link.primary || s==i))) {

	/* coming from downstream -> forward/send to interfaces using this interface upstream */

	if (i->flags & _INTERFACE_FLAG_PRIMARY_1TO1) {
	    struct context_interface_s *tmp=i->link.secondary.interface;

	    return (* tmp->iocmd.in)(tmp, what, option, i, type);

	} else if (i->flags & _INTERFACE_FLAG_PRIMARY_1TON) {
	    struct select_sftp2ctx_hlpr_s hlpr;

	    hlpr.i=i;
	    return signal_selected_ctx(i, 0, what, option, type, select_sftp2ctx_cb, (void *) &hlpr);

	}

    } else if (i->flags & (_INTERFACE_FLAG_SECONDARY_1TO1 | _INTERFACE_FLAG_SECONDARY_1TON)) {
	struct context_interface_s *primary=i->link.primary;

	/* coming from upstream -> send downstream */

	return (* primary->iocmd.in)(primary, what, option, s, type);

    }

    return 0;
}

static int _signal_ctx2sftp_primary(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type)
{
    char *buffer=(* i->get_interface_buffer)(i);
    struct sftp_client_s *sftp=(struct sftp_client_s *) buffer;
    return (* sftp->context.signal_ctx2sftp)(&sftp, what, option, type);
}

/* function called by the sftp client to inform the context of events */

static int _signal_sftp2ctx_primary(struct sftp_client_s *sftp, const char *what, struct io_option_s *option, unsigned int type)
{
    struct context_interface_s *i=(struct context_interface_s *)((char *) sftp - offsetof(struct context_interface_s, buffer));
    return (* i->iocmd.out)(i, what, option, i, type);
}

static void recv_data_ssh_channel_sftp_client(struct ssh_channel_s *channel, char **buffer, unsigned int size, uint32_t seq, unsigned char ssh_flags)
{
    struct sftp_client_s *sftp=get_sftp_client_ssh_channel(channel);
    unsigned int sftp_flags=0;

    logoutput_debug("recv_data_ssh_channel_sftp_client: size %u channel %i", size, ((channel) ? channel->local_channel : -1));

    sftp_flags |= (ssh_flags & _CHANNEL_DATA_RECEIVE_FLAG_ALLOC) ? SFTP_RECEIVE_FLAG_ALLOC : 0;
    sftp_flags |= (ssh_flags & _CHANNEL_DATA_RECEIVE_FLAG_ERROR) ? SFTP_RECEIVE_FLAG_ERROR : 0;
    (* sftp->context.recv_data)(sftp, buffer, size, seq, sftp_flags);

}

static int _send_data_ssh_channel_sftp_client(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);
    // logoutput_debug("_send_data_ssh_channel_sftp_client: size %u channel %i", size, ((channel) ? channel->local_channel : -1));
    return send_channel_data_message(channel, buffer, size, seq);
}

/* pair the time correction functions ... these are valid per ssh session */

static void _correct_time_c2s(struct sftp_client_s *sftp, struct system_timespec_s *time)
{
    struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);
    struct ssh_session_s *session=channel->session;
    (* session->hostinfo.correct_time_c2s)(session, time);
}

static void _correct_time_s2c(struct sftp_client_s *sftp, struct system_timespec_s *time)
{
    struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);
    struct ssh_session_s *session=channel->session;
    (* session->hostinfo.correct_time_s2c)(session, time);
}

static char *get_interface_buffer_secondary(struct context_interface_s *i)
{
    struct context_interface_s *primary=i->link.primary;
    return primary->buffer;
}

/*
	INTERFACE OPS
			*/

static unsigned int _populate_interface_sftp_client(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start)
{

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SFTP_CLIENT;
	ilist[start].name="sftp";
	ilist[start].ops=ops;

    }
    start++;
    return start;
}

static unsigned int _get_interface_buffer_size_sftp_client(struct interface_list_s *ilist)
{
    unsigned int size=0;

    if ((ilist->type==_INTERFACE_TYPE_SFTP_CLIENT) && (strcmp(ilist->name, "sftp")==0)) {

	size=get_sftp_buffer_size();

    }

    return size;

}

static int _init_interface_buffer_sftp_client(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct sftp_client_s *sftp=NULL;
    struct ssh_channel_s *channel=NULL;
    struct ssh_session_s *session=NULL;
    struct service_context_s *context=get_service_context(interface);

    if (strcmp(ilist->name, "sftp")!=0) {

	logoutput_warning("_init_interface_buffer_sftp_client: wring interface %s", ilist->name);
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_sftp_client: buffer already initialized");
	return 0;

    }

    logoutput("_init_interface_buffer_sftp_client");

    if (primary) {

	if (primary->type==_INTERFACE_TYPE_SSH_CHANNEL) {

	    /* there is a 1:1 between the ssh channel and the sftp client */

	    channel=(struct ssh_channel_s *) primary->buffer;
	    primary->link.secondary.interface=interface;
	    primary->flags |= _INTERFACE_FLAG_PRIMARY_1TO1;
	    interface->link.primary=primary;
	    interface->flags |= _INTERFACE_FLAG_SECONDARY_1TO1;

	} else if (primary->type==_INTERFACE_TYPE_SFTP_CLIENT) {

	    /* there is a 1:n relation between shared sftp client and sftp client in browse environment */

	    primary->link.secondary.refcount++;
	    primary->flags |= _INTERFACE_FLAG_PRIMARY_1TON;

	    interface->link.primary=primary;
	    interface->flags |= _INTERFACE_FLAG_SECONDARY_1TON;

	    interface->iocmd.in = _signal_ctx2sftp_secondary;
	    interface->connect=_connect_interface_client_sftp_client;
	    interface->start=_start_interface_client_sftp_client;
	    interface->get_interface_buffer=get_interface_buffer_secondary;
	    return 0;

	}

    }

    if (channel==NULL) {

	logoutput("_init_interface_buffer_sftp_client: channel not defined");
	return -1;

    }

    interface->type=_INTERFACE_TYPE_SFTP_CLIENT;
    interface->flags |= _INTERFACE_FLAG_PRIMARY_1TON;
    sftp=(struct sftp_client_s *) interface->buffer;
    session=channel->session;

    if (init_sftp_client(sftp, session->identity.pwd.pw_uid, &session->hostinfo.mapping)>=0) {

	interface->connect=_connect_interface_shared_sftp_client;
	interface->start=_start_interface_shared_sftp_client;

	interface->iocmd.in=_signal_ctx2sftp_primary;
	sftp->context.signal_sftp2ctx=_signal_sftp2ctx_primary;

	sftp->signal.signal=channel->signal;

	/* set the time correction functions for the sftp client */

	sftp->time_ops.correct_time_c2s=_correct_time_c2s;
	sftp->time_ops.correct_time_s2c=_correct_time_s2c;

	/* pair the sending from sftp to the channel, and the
	    receiving by the channel to the sftp client */

	sftp->context.send_data=_send_data_ssh_channel_sftp_client;
	channel->context.recv_data=recv_data_ssh_channel_sftp_client;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	return 0;

    }

    out:
    return -1;

}

static void _clear_interface_buffer_sftp_client(struct context_interface_s *i)
{

    if (i->ptr) {
	struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;

	free(data);
	i->ptr=NULL;

    }

    clear_interface_buffer_default(i);

}

static struct interface_ops_s sftp_interface_ops = {
    .name					= "SSH_SUBSYSTEM",
    .populate					= _populate_interface_sftp_client,
    .get_buffer_size				= _get_interface_buffer_size_sftp_client,
    .init_buffer				= _init_interface_buffer_sftp_client,
    .clear_buffer				= _clear_interface_buffer_sftp_client,
};

void init_sftp_client_interface()
{
    add_interface_ops(&sftp_interface_ops);
}

int send_sftp_statvfs_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    unsigned int index=data->statvfs_index;
    return send_sftp_extension_index(sftp, index, sftp_r);
}

unsigned int get_index_sftp_extension_statvfs(struct context_interface_s *i)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    struct ssh_string_s name=SSH_STRING_SET(0, "statvfs@openssh.com");
    unsigned int index=get_sftp_protocol_extension_index(sftp, &name);
    logoutput_debug("get_index_sftp_extension_statvfs: index %u", index);
    return index;
}

int send_sftp_fsync_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    unsigned int index=data->fsync_index;
    return send_sftp_extension_index(sftp, index, sftp_r);
}

unsigned int get_index_sftp_extension_fsync(struct context_interface_s *i)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct ssh_string_s name=SSH_STRING_SET(0, "fsync@openssh.com");
    unsigned int index=get_sftp_protocol_extension_index(sftp, &name);
    logoutput_debug("get_index_sftp_extension_fsync: index %u", index);
    return index;
}

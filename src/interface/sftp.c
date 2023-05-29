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

#include "ssh/ssh-common.h"
#include "sftp/common.h"
#include "sftp/recv.h"
#include "ssh/send/msg-channel.h"
#include "ssh-utils.h"
#include "ssh/ssh-channel.h"

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
    unsigned int				fstatat_index;
    int						(* compare_path)(struct context_interface_s *i, char *path, unsigned int len, unsigned int type);
    unsigned int				(* get_complete_pathlen)(struct context_interface_s *i, struct fuse_path_s *fp);
    unsigned char				type;
    unsigned int				len;
    char					path[];
};

static unsigned int get_complete_pathlen_home(struct context_interface_s *i, struct fuse_path_s *fpath)
{
    fpath->pathstart++;
    logoutput_debug("get_complete_pathlen_home: %u:%s", i->unique, fpath->pathstart);
    return (unsigned int)(fpath->path + fpath->len - 1 - fpath->pathstart);
}

char *get_ssh_session_remote_home(struct context_interface_s *i)
{
    char *home=NULL;
    struct context_interface_s *primary=i->link.primary;
    struct service_context_s *ctx=get_service_context(i);

    /* walk back to root == ssh session */

    while (((i->type==_INTERFACE_TYPE_SFTP_CLIENT) && (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM)) ||
            (((i->type==_INTERFACE_TYPE_SFTP_CLIENT) || (i->type==_INTERFACE_TYPE_SSH_CHANNEL)) && (ctx->type==SERVICE_CTX_TYPE_SHARED))) {

        if (primary==NULL) break;

        i=primary;
        ctx=get_service_context(i);
        primary=i->link.primary;

    }

    if (i->type==_INTERFACE_TYPE_SSH_SESSION) {
        struct ssh_session_s *session=(struct ssh_session_s *) (* i->get_interface_buffer)(i);
	struct net_idmapping_s *mapping=&session->hostinfo.mapping;

	home=mapping->su.type.user.home;

    }

    return home;
}

static int compare_path_home(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    int result=-1;

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
    logoutput_debug("get_complete_pathlen_root: %u:%s", i->unique, fpath->pathstart);
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

    fpath->pathstart -= data->len;
    memcpy(fpath->pathstart, data->path, data->len);
    logoutput_debug("get_complete_pathlen_custom: %u:%s", i->unique, fpath->pathstart);
    return (unsigned int)(fpath->path + fpath->len - 1 - fpath->pathstart);
}

static int compare_path_custom(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    int result=-1;

    switch (type) {

	case SFTP_COMPARE_PATH_PREFIX_SUBDIR:

	    if ((len > data->len) && (strncmp(path, data->path, data->len)==0) && (strncmp(&path[data->len], "/", 1)==0)) result=(int) data->len;
	    break;

	default:

	    logoutput_debug("compare_path_custom: type %u not reckognized", type);

    }

    return result;
}

static struct sftp_client_data_s home_data_fallback = {
    .flags                                              = 0,
    .statvfs_index                                      = 0,
    .fsync_index                                        = 0,
    .fstatat_index                                      = 0,
    .compare_path                                       = compare_path_home,
    .get_complete_pathlen                               = get_complete_pathlen_home,
    .type                                               = SFTP_PREFIX_TYPE_HOME,
    .len                                                = 0,
};

struct ssh_channel_s *get_ssh_channel_sftp_client(struct sftp_client_s *sftp)
{
    struct context_interface_s *i=(struct context_interface_s *)((char *) sftp - offsetof(struct context_interface_s, buffer));
    struct context_interface_s *primary=i->link.primary;
    char *buffer=(* primary->get_interface_buffer)(primary);

    return (struct ssh_channel_s *) buffer;
}

struct sftp_client_s *get_sftp_client_ssh_channel(struct ssh_channel_s *channel)
{
    struct context_interface_s *i=(struct context_interface_s *)((char *) channel - offsetof(struct context_interface_s, buffer));
    struct context_interface_s *secondary=i->link.secondary.interface;

    return (struct sftp_client_s *) secondary->buffer;
}

static int get_index_sftp_extension_common(struct context_interface_s *i, char *name)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* i->get_interface_buffer)(i);
    struct ssh_string_s tmp=SSH_STRING_SET(0, name);
    unsigned int index=get_sftp_protocol_extension_index(sftp, &tmp);
    return index;
}

/* char *home=get_ssh_session_remote_home(i); */

static int set_prefix_sftp_client(struct context_interface_s *i, char *prefix, char *home)
{
    struct sftp_client_data_s *data=NULL;
    unsigned int len=0;

    if (prefix) {

        if (home) {
	    unsigned int tmp=((home) ? strlen(home) : 0);

	    logoutput_debug("set_prefix_sftp_client: remote home %s prefix %s", home, prefix);

	    len=(((strcmp(prefix, "~")==0) ||
	        ((tmp>0) && (strcmp(prefix, home)==0)) ||
	        ((tmp>0) && (strlen(prefix) == tmp + 1) && (strncmp(prefix, home, tmp)==0) && (prefix[tmp]=='/'))) ? 0 : strlen(prefix));

        } else {

            logoutput_debug("set_prefix_sftp_client: prefix %s (home not set)", prefix);
            len=strlen(prefix);

        }

    } else {

	len=0;

    }

    data=malloc(sizeof(struct sftp_client_data_s) + len);

    if (data) {

	i->ptr=(void *) data;

	data->flags=0;
	data->statvfs_index=get_index_sftp_extension_common(i, "statvfs@openssh.com");
	data->fsync_index=get_index_sftp_extension_common(i, "fsync@openssh.com");
	data->fstatat_index=get_index_sftp_extension_common(i, "fstatat@sftp.osns.net");

	if (len==0) {

	    /* remote subdirectory is the user's homedirectory
		now by protocol when a path is send to the sftp server
		and does not start with a slash it's relative to $HOME on server */

	    data->type=SFTP_PREFIX_TYPE_HOME;
	    data->compare_path=compare_path_home;
	    data->get_complete_pathlen=get_complete_pathlen_home;
	    data->len=0;

            logoutput_debug("set_prefix_sftp_client: set i %u home", i->unique);

	} else if (len==1 && strcmp(prefix, "/")==0) {

	    /* remote subdirectory is root: path's are send without modification */

	    data->type=SFTP_PREFIX_TYPE_ROOT;
	    data->compare_path=compare_path_root;
	    data->get_complete_pathlen=get_complete_pathlen_root;
	    memcpy(data->path, "/", 1);
	    data->len=1;

            logoutput_debug("set_prefix_sftp_client: set i %u root", i->unique);

	} else {

	    /* remote subdirectory is custom: every path send is prepended by this */

	    data->type=SFTP_PREFIX_TYPE_CUSTOM;
	    data->compare_path=compare_path_custom;
	    data->get_complete_pathlen=get_complete_pathlen_custom;
	    memcpy(data->path, prefix, len);
	    data->len=len;

            logoutput_debug("set_prefix_sftp_client: set i %u custom %.*s", i->unique, len, prefix);

	}

    }

    return ((data) ? 0 : -1);

}

int sftp_compare_path(struct context_interface_s *i, char *path, unsigned int len, unsigned int type)
{
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    return (* data->compare_path)(i, path, len, type);
}

/* PATH buffer allocation and conversion (local->protocol and vice versa) */

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

/* io between channel and sftp client */

static void handle_data_ssh_channel_sftp_client(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;
    struct sftp_client_s *sftp=get_sftp_client_ssh_channel(channel);

    /* forward data to sftp client */

    (* sftp->context.recv_data)(sftp, payload->buffer, payload->len, payload->sequence, 0);
}

static void handle_xdata_ssh_channel_sftp_client(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;

    if (payload->code==SSH_EXTENDED_DATA_STDERR) {

        replace_cntrl_char(payload->buffer, payload->len, REPLACE_CNTRL_FLAG_UNDERSCORE | REPLACE_CNTRL_FLAG_TEXT);
        logoutput_debug("handle_xdata_ssh_channel_sftp_client: received stderr %.*s", payload->len, payload->buffer);

    } else {

        logoutput_debug("handle_xdata_ssh_channel_sftp_client: received xdata code %u", payload->code);

        /* extra filesystem services like:
            - fs change notify
            - quota
        */

    }

}

static void handle_open_ssh_channel_sftp_client(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;

    if ((payload->type==SSH_MSG_CHANNEL_EOF) || (payload->type==SSH_MSG_CHANNEL_CLOSE)) {

        logoutput_debug("handle_open_ssh_channel_sftp_client: received %s", ((payload->type==SSH_MSG_CHANNEL_EOF) ? "eof" : "close"));

    } else {

        logoutput_debug("handle_open_ssh_channel_sftp_client: received msg type %u", payload->type);

    }

}

static void handle_request_ssh_channel_sftp_client(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;
    struct ssh_string_s request=SSH_STRING_INIT;

    if (read_ssh_string(payload->buffer, payload->len, &request)>0) {

        logoutput_debug("handle_request_ssh_channel_sftp_client: received request %.*s", request.len, request.ptr);

    }

}

/* interface callbacks SFTP client */

static int _connect_interface_sftp_client(struct context_interface_s *i, union interface_target_u *target, union interface_parameters_u *param)
{
    struct service_context_s *ctx=get_service_context(i);
    int result=-1;

    logoutput_debug("_start_interface_sftp_client");

    if (i->flags & _INTERFACE_FLAG_CONNECT) {

        logoutput_debug("_start_interface_sftp_client: already started");
        return 0;

    }

    if (ctx->type==SERVICE_CTX_TYPE_SHARED) {
	struct context_interface_s *primary=(* i->get_primary)(i);

	if (primary && primary->type==_INTERFACE_TYPE_SSH_CHANNEL) {
	    struct ssh_channel_s *channel=(struct ssh_channel_s *) primary->buffer;

	    /* test the channel is suitable to use ...
	        TODO: add direct-tcpip and direct-streamlocal channels */

            logoutput_debug("_start_interface_sftp_client: found ssh channel %u type %u", channel->lcnr, channel->type);

	    if (match_ssh_channel_uri(channel, "ssh-channel://session/subsystem/sftp")==0) {

	        result=ssh_channel_start_command(channel, SSH_CHANNEL_START_COMMAND_FLAG_REPLY, NULL, NULL);

                if (result>=0) {

                    channel->iocb[SSH_CHANNEL_IOCB_RECV_DATA]=handle_data_ssh_channel_sftp_client;
                    channel->iocb[SSH_CHANNEL_IOCB_RECV_OPEN]=handle_open_ssh_channel_sftp_client;
                    channel->iocb[SSH_CHANNEL_IOCB_RECV_XDATA]=handle_xdata_ssh_channel_sftp_client;
                    channel->iocb[SSH_CHANNEL_IOCB_RECV_REQUEST]=handle_request_ssh_channel_sftp_client;

                }

            }

        }

    } else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

        result=0;

    }

    if (result==0) i->flags |= _INTERFACE_FLAG_CONNECT;
    return result;

}

static int _start_interface_sftp_client(struct context_interface_s *i)
{
    struct service_context_s *ctx=get_service_context(i);
    int result=-1;

    if (i->flags & _INTERFACE_FLAG_START) {

        logoutput_debug("_start_interface_sftp_client: already started");
        return 0;

    }

    if (ctx->type==SERVICE_CTX_TYPE_SHARED) {
        char *buffer=(* i->get_interface_buffer)(i);
        struct sftp_client_s *sftp=(struct sftp_client_s *) buffer;
        struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);

        /* switch the processing if incoming data for the channel to the sftp subsystem (=context)
	    it's not required to set the cb here...it's already set in the init phase */



        /* pair the sending from sftp to the channel, and the
	    receiving by the channel to the sftp client */

        result=start_init_sftp_client(sftp);

    } else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

        result=((i->ptr) ? 0 : -1);

    }

    if (result==0) i->flags |= _INTERFACE_FLAG_START;
    return result;

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

static unsigned char break_request_sftp_client(struct sftp_client_s *sftp, unsigned int *p_status)
{
    struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);
    struct ssh_connection_s *connection=channel->connection;
    unsigned char dobreak=0;

    /* waiting threads involving a sftp request should be informed
	when the ssh channel taking care for the connection is closed/eof/exit */

    if ((channel->flags & (SSH_CHANNEL_FLAG_SERVER_EOF | SSH_CHANNEL_FLAG_SERVER_CLOSE | SSH_CHANNEL_FLAG_EXIT_SIGNAL | SSH_CHANNEL_FLAG_EXIT_STATUS))) {

	*p_status |= SFTP_REQUEST_STATUS_DISCONNECT;
	dobreak=1;

    }

    return dobreak;
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

static unsigned int get_interface_status(struct context_interface_s *i, struct interface_status_s *status)
{
    struct service_context_s *ctx=get_service_context(i);
    unsigned int tmp=0;

    if ((ctx->type==SERVICE_CTX_TYPE_SHARED) || (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM)) {
        struct context_interface_s *primary=i->link.primary;

        if (primary) tmp=(* primary->get_interface_status)(primary, status);

    }

    return tmp;
}

static void link_interface_sftp_client(struct context_interface_s *i, struct context_interface_s *primary)
{
    struct service_context_s *ctx=get_service_context(i);

    if (primary==NULL) {

        logoutput_debug("link_interface_sftp_client: no primary ... reset");
        i->link.primary=NULL;
        i->flags &= ~_INTERFACE_FLAG_SECONDARY;
        return;

    }

    if (ctx->type==SERVICE_CTX_TYPE_SHARED) {

        /* primary has to be a ssh channel */

        if (primary->type==_INTERFACE_TYPE_SSH_CHANNEL) {
            char *buffer=(* primary->get_interface_buffer)(primary);
            struct ssh_channel_s *channel=(struct ssh_channel_s *) buffer;

            /* a one to one relation between a sftp client (connected to sftp subsystem) and a ssh channel */

	    primary->link.secondary.interface=i;
	    primary->flags |= _INTERFACE_FLAG_PRIMARY_1TO1;
	    i->link.primary=primary;
	    i->flags |= _INTERFACE_FLAG_SECONDARY_1TO1;
	    logoutput_debug("link_interface_sftp_client: link to ssh channel (%u:%u)", channel->lcnr, channel->rcnr);

        } else {

            logoutput_warning("link_interface_sftp_client: interface of type shared sftp client, but primary not a ssh channel (interface type=%u)", primary->type);

        }

    } else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

        /* primary has to be a sftp shared client */

        if (primary->type==_INTERFACE_TYPE_SFTP_CLIENT) {

            ctx=get_service_context(primary);

            if (ctx->type==SERVICE_CTX_TYPE_SHARED) {

                primary->link.secondary.refcount++;
                primary->flags |= _INTERFACE_FLAG_PRIMARY_1TON;

                i->link.primary=primary;
                i->flags |= _INTERFACE_FLAG_SECONDARY_1TON;

                i->iocmd.in = _signal_ctx2sftp_secondary;
                i->get_interface_buffer=get_interface_buffer_secondary;
                logoutput_debug("link_interface_sftp_client: link to shared sftp client");

            } else {

                logoutput_warning("link_interface_sftp_client: interface of type browse sftp client, but primary not a shared sftp client (ctx type=%u)", ctx->type);

            }

        } else {

            logoutput_warning("link_interface_sftp_client: interface of type browse sftp client, but primary not a sftp client (interface type=%u)", primary->type);

        }

    }

}



static int send_data_ssh_channel_sftp_client(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    struct ssh_channel_s *channel=get_ssh_channel_sftp_client(sftp);
    union ssh_message_u msg;

    memset(&msg, 0, sizeof(union ssh_message_u));

    msg.channel.type.data.len=size;
    msg.channel.type.data.data=buffer;
    return send_ssh_channel_data_msg(channel, &msg);

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

static unsigned int _get_interface_buffer_size_sftp_client(struct interface_list_s *ilist, struct context_interface_s *p)
{
    unsigned int size=0;

    if ((ilist->type==_INTERFACE_TYPE_SFTP_CLIENT) && (strcmp(ilist->name, "sftp")==0)) {

	size=get_sftp_buffer_size();

    }

    return size;

}

static int _init_interface_buffer_sftp_client(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct ssh_channel_s *channel=NULL;
    struct service_context_s *context=get_service_context(interface);

    if (strcmp(ilist->name, "sftp")!=0) {

	logoutput_warning("_init_interface_buffer_sftp_client: wrong interface %s", ilist->name);
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_sftp_client: buffer already initialized");
	return 0;

    }

    /*interface->start=_start_interface_browse_sftp_client;
    interface->get_interface_buffer=get_interface_buffer_secondary;
    interface->get_interface_status=get_interface_status_secondary;
    interface->connect=_connect_interface_browse_sftp_client;
    */

    interface->type=_INTERFACE_TYPE_SFTP_CLIENT;
    interface->connect=_connect_interface_sftp_client;
    interface->start=_start_interface_sftp_client;
    interface->get_interface_status=get_interface_status;
    interface->ptr=&home_data_fallback; /* make sure there is allways a sftp data to fallback on ... later a custom can be set */
    interface->set_primary=link_interface_sftp_client;

    if (primary) {

        link_interface_sftp_client(interface, primary);

	if (primary->type==_INTERFACE_TYPE_SSH_CHANNEL) {

	    /* there is a 1:1 between the ssh channel and the sftp client */

            logoutput_debug("_init_interface_buffer_sftp_client: primary ssh channel");

	    channel=(struct ssh_channel_s *) primary->buffer;

	} else if (primary->type==_INTERFACE_TYPE_SFTP_CLIENT) {

            logoutput_debug("_init_interface_buffer_sftp_client: primary sftp client");
	    return 0;

	}

    } else {

        logoutput_debug("_init_interface_buffer_sftp_client: no primary: standalone");
        link_interface_sftp_client(interface, NULL);
        return 0;

    }

    if (channel) {
        struct ssh_session_s *session=channel->session;
        struct sftp_client_s *sftp=(struct sftp_client_s *) interface->buffer;

        if (init_sftp_client(sftp, session->identity.pwd.pw_uid, &session->hostinfo.mapping)>=0) {

	    interface->iocmd.in=_signal_ctx2sftp_primary;
	    sftp->context.signal_sftp2ctx=_signal_sftp2ctx_primary;
	    sftp->signal.signal=channel->signal;
	    sftp->context.break_request=break_request_sftp_client;
	    sftp->context.unique=interface->unique;

	    /* set the time correction functions for the sftp client */

	    sftp->time_ops.correct_time_c2s=_correct_time_c2s;
	    sftp->time_ops.correct_time_s2c=_correct_time_s2c;

            sftp->context.send_data=send_data_ssh_channel_sftp_client;

	    interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	    return 0;

        }

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
    .name					= "SFTP-CLIENT",
    .populate					= _populate_interface_sftp_client,
    .get_buffer_size				= _get_interface_buffer_size_sftp_client,
    .init_buffer				= _init_interface_buffer_sftp_client,
    .clear_buffer				= _clear_interface_buffer_sftp_client,
};

void init_sftp_client_interface()
{
    add_interface_ops(&sftp_interface_ops);
}

static struct sftp_client_data_s *get_sftp_client_data(struct context_interface_s *i)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (struct sftp_client_data_s *) i->ptr;
}

int send_sftp_statvfs_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    return send_sftp_extension_index(sftp, data->statvfs_index, sftp_r);
}

unsigned int get_index_sftp_extension_statvfs(struct context_interface_s *i)
{
    struct sftp_client_data_s *data=get_sftp_client_data(i);
    return data->statvfs_index;
}

int send_sftp_fsync_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    return send_sftp_extension_index(sftp, data->fsync_index, sftp_r);
}

unsigned int get_index_sftp_extension_fsync(struct context_interface_s *i)
{
    struct sftp_client_data_s *data=get_sftp_client_data(i);
    return data->fsync_index;
}

int send_sftp_fstatat_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    struct sftp_client_data_s *data=(struct sftp_client_data_s *) i->ptr;
    return send_sftp_extension_index(sftp, data->fstatat_index, sftp_r);
}

unsigned int get_index_sftp_extension_fstatat(struct context_interface_s *i)
{
    struct sftp_client_data_s *data=get_sftp_client_data(i);
    return data->fstatat_index;
}

void set_primary_sftp_client(struct context_interface_s *i, struct context_interface_s *primary)
{

    if (i && i->type==_INTERFACE_TYPE_SFTP_CLIENT) link_interface_sftp_client(i, primary);

}

int set_prefix_sftp_browse_client(struct context_interface_s *i, char *prefix, char *home)
{

    if (i) {
        struct service_context_s *ctx=get_service_context(i);

        if ((i->type==_INTERFACE_TYPE_SFTP_CLIENT) && (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM)) {

            if ((i->ptr != &home_data_fallback)) {

                return 0;

            } else if (set_prefix_sftp_client(i, prefix, home)==0) {

                return 1;

            }

        }

    }

    return -1;

}

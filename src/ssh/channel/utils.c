/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "ssh-common.h"
#include "ssh-channel.h"

#include "ssh-hostinfo.h"
#include "ssh-utils.h"

#include "receive/msg-channel.h"
#include "receive/payload.h"
#include "send/msg-channel.h"

static const char *openfailure_reasons[] = {
	"Open administratively prohibited.",
	"Open connect failed.", 
	"Open unknown channel type.",
	"Open resource shortage."};

const char *get_ssh_channel_open_failure_reason(unsigned int reason)
{
    if (reason > 0 && reason <= (sizeof(openfailure_reasons) / sizeof(openfailure_reasons[0]))) return openfailure_reasons[reason-1];
    return "Open unknown failure.";
}

static void get_ssh_channel_expire_shared(struct ssh_channel_s *channel, struct system_timespec_s *expire, unsigned int sec)
{
    get_current_time_system_time(expire);
    system_time_add(expire, SYSTEM_TIME_ADD_ZERO, sec);
}

void get_ssh_channel_expire_custom(struct ssh_channel_s *channel, struct system_timespec_s *expire)
{
    get_ssh_channel_expire_shared(channel, expire, 4);
}

void get_ssh_channel_expire_init(struct ssh_channel_s *channel, struct system_timespec_s *expire)
{
    get_ssh_channel_expire_shared(channel, expire, 4);
}

unsigned int get_ssh_channel_interface_info(struct ssh_channel_s *channel, char *buffer, unsigned int size)
{
    unsigned int result=0;

    if (size>=4) {

	memset(buffer, '\0', size);

	if (channel->flags & SSH_CHANNEL_FLAG_OPENFAILURE) {

	    store_uint32(buffer, EFAULT);
	    result=4;

	} else if (channel->flags & (SSH_CHANNEL_FLAG_SERVER_EOF | SSH_CHANNEL_FLAG_CLIENT_EOF)) {

	    store_uint32(buffer, ENODEV); /* connected with server but backend on server not */
	    result=4;

	} else {
	    struct connection_s *c=&channel->connection->connection;

	    if (c->sock.status & (SOCKET_STATUS_CLOSED | SOCKET_STATUS_CLOSING)) {

		store_uint32(buffer, ENOTCONN); /* not connected with server */
		result=4;

	    }

	}

    }

    return result;

}

unsigned int get_ssh_channel_exit_signal(struct ssh_string_s *name)
{
    unsigned int exit_signal=0;

    if (compare_ssh_string(name, 'c', "ABRT")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_ABRT;

    } else if (compare_ssh_string(name, 'c', "ALRM")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_ALRM;

    } else if (compare_ssh_string(name, 'c', "FPE")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_FPE;

    } else if (compare_ssh_string(name, 'c', "HUP")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_HUP;

    } else if (compare_ssh_string(name, 'c', "ILL")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_ILL;

    } else if (compare_ssh_string(name, 'c', "INT")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_INT;

    } else if (compare_ssh_string(name, 'c', "KILL")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_KILL;

    } else if (compare_ssh_string(name, 'c', "PIPE")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_PIPE;

    } else if (compare_ssh_string(name, 'c', "QUIT")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_QUIT;

    } else if (compare_ssh_string(name, 'c', "SEGV")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_SEGV;

    } else if (compare_ssh_string(name, 'c', "TERM")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_TERM;

    } else if (compare_ssh_string(name, 'c', "USR1")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_USR1;

    } else if (compare_ssh_string(name, 'c', "USR2")==0) {

	exit_signal=SSH_CHANNEL_EXIT_SIGNAL_USR2;

    }

    return exit_signal;

}

struct ssh_channel_s *create_ssh_session_channel(struct ssh_session_s *session, const char *what, char *command)
{
    struct ssh_channel_s *channel=NULL;
    unsigned int len=((command) ? strlen(command) : 0);
    unsigned char type=0;

    if ((strcmp(what, "subsystem")==0) || (strcmp(what, "exec")==0)) {

	if (len==0 || len>=SSH_CHANNEL_SESSION_BUFFER_MAXLEN) return NULL;
	type=((strcmp(what, "exec")==0) ? SSH_CHANNEL_SESSION_TYPE_EXEC : SSH_CHANNEL_SESSION_TYPE_SUBSYSTEM);

    } else if (strcmp(what, "shell")==0) {

	len=0;
	type=SSH_CHANNEL_SESSION_TYPE_SHELL;

    } else {

	return NULL;

    }

    channel=allocate_ssh_channel(session, session->connections.main, SSH_CHANNEL_TYPE_SESSION);
    if (channel==NULL) return NULL;

    if (len>0) {

	memset(channel->target.session.buffer, 0, SSH_CHANNEL_SESSION_BUFFER_MAXLEN);
	memcpy(channel->target.session.buffer, command, len);

    }

    channel->type = SSH_CHANNEL_TYPE_SESSION;
    channel->target.session.type=type;

    if (add_ssh_channel(channel, SSH_CHANNEL_FLAG_OPEN)==-1) {

	remove_ssh_channel(channel, SSH_CHANNEL_FLAG_CLIENT_CLOSE, 0);
	free_ssh_channel(&channel);
	logoutput_debug("create_ssh_session_channel: unable to add channel");

    }

    return channel;

}

struct _cb_get_payload_hlpr_s {
    struct ssh_channel_s 						*channel;
    unsigned int							flags;
    void                                                                (* cb)(struct ssh_channel_s *c, struct ssh_payload_s **p, unsigned int flags, unsigned int errcode, void *ptr);
    void                                                                *ptr;
};

static int _cb_select_payload(struct ssh_payload_s *payload, void *ptr)
{
    struct _cb_get_payload_hlpr_s *hlpr=(struct _cb_get_payload_hlpr_s *) ptr;
    int result=0;

    logoutput_debug("_cb_select_payload: type %u", payload->type);

    if (hlpr->flags & SSH_CHANNEL_START_COMMAND_FLAG_UNEXPECTED) {

        result=1;

    } else if ((payload->type==SSH_MSG_CHANNEL_SUCCESS) || (payload->type==SSH_MSG_CHANNEL_FAILURE)) {

	result=1;

    } else if ((hlpr->flags & SSH_CHANNEL_START_COMMAND_FLAG_DATA) && ((payload->type==SSH_MSG_CHANNEL_DATA) || (payload->type==SSH_MSG_CHANNEL_EXTENDED_DATA))) {

	result=1;

    }

    return result;

}

static unsigned char _cb_break(void *ptr)
{
    struct _cb_get_payload_hlpr_s *hlpr=(struct _cb_get_payload_hlpr_s *) ptr;
    struct ssh_channel_s *channel=hlpr->channel;
    struct ssh_connection_s *sshc=channel->connection;

    return ((((channel->flags & SSH_CHANNEL_FLAG_SERVER_CLOSE) || (sshc->receive.status & SSH_RECEIVE_STATUS_DISCONNECT)) && (channel->queue.header.count==0)) ? 1 : 0);
}

static void _cb_error(unsigned int errcode, void *ptr)
{
    struct _cb_get_payload_hlpr_s *hlpr=(struct _cb_get_payload_hlpr_s *) ptr;

    (* hlpr->cb)(NULL, NULL, SSH_CHANNEL_START_COMMAND_FLAG_ERROR, errcode, hlpr->ptr);
}

static void _cb_start_channel_default(struct ssh_channel_s *c, struct ssh_payload_s **p, unsigned int flags, unsigned int errcode, void *ptr)
{
    logoutput_debug("_cb_start_channel_default: flags %u errcode %u", flags, errcode);
}

int ssh_channel_start_command(struct ssh_channel_s *channel, unsigned int flags, void (* cb)(struct ssh_channel_s *c, struct ssh_payload_s **p, unsigned int flags, unsigned int errcode, void *ptr), void *ptr)
{
    unsigned int errcode=0;
    uint32_t seq;
    int result=-1;

    logoutput_debug("ssh_channel_start_command");

    if (cb==NULL) cb=_cb_start_channel_default;

    if (channel==NULL || (channel->type != SSH_CHANNEL_TYPE_SESSION)) {

	(* cb)(NULL, NULL, SSH_CHANNEL_START_COMMAND_FLAG_ERROR, EINVAL, ptr);
	return -1;

    }

    result=send_ssh_channel_start_command_msg(channel, (flags & SSH_CHANNEL_START_COMMAND_FLAG_REPLY));

    if ((result==-1) || (channel->flags & (SSH_CHANNEL_FLAG_SERVER_CLOSE | SSH_CHANNEL_FLAG_SERVER_EOF | SSH_CHANNEL_FLAG_OPENFAILURE))) {

	logoutput_debug("ssh_channel_start_command: unable to send start command message");
	(* cb)(channel, NULL, SSH_CHANNEL_START_COMMAND_FLAG_ERROR, EIO, ptr); /* EIO? a better error?*/
	goto out;

    }

    /* if reply is requested */

    if (flags & SSH_CHANNEL_START_COMMAND_FLAG_REPLY) {
	struct system_timespec_s expire=SYSTEM_TIME_INIT;
	struct ssh_payload_s *payload=NULL;
	struct _cb_get_payload_hlpr_s hlpr;

	result=-1;

	hlpr.channel=channel;
	hlpr.flags=SSH_CHANNEL_START_COMMAND_FLAG_REPLY;
	hlpr.cb=cb;
	hlpr.ptr=ptr;

	get_ssh_channel_expire_shared(channel, &expire, 4);

	while ((channel->flags & SSH_CHANNEL_FLAG_CLIENT_CLOSE)==0) {

	    payload=get_ssh_payload(&channel->queue, &expire, _cb_select_payload, _cb_break, _cb_error, (void *) &hlpr);

	    if (payload) {

		logoutput_debug("ssh_channel_start_command: received message type %u", payload->type);

		if (payload->type==SSH_MSG_CHANNEL_SUCCESS) {

		    result=0;
		    (* cb)(channel, &payload, SSH_CHANNEL_START_COMMAND_FLAG_REPLY, 0, ptr);
		    break;

		} else if (payload->type==SSH_MSG_CHANNEL_FAILURE) {

		    (* cb)(channel, &payload, SSH_CHANNEL_START_COMMAND_FLAG_REPLY, 0, ptr);
		    break;

		}

		if (payload) free_payload(&payload);

	    }

	}

    }

    /* process output/data when requested */

    if (flags & SSH_CHANNEL_START_COMMAND_FLAG_DATA) {
	struct system_timespec_s expire=SYSTEM_TIME_INIT;
	struct ssh_payload_s *payload=NULL;
	struct _cb_get_payload_hlpr_s hlpr;

	hlpr.channel=channel;
	hlpr.flags=(SSH_CHANNEL_START_COMMAND_FLAG_DATA | SSH_CHANNEL_START_COMMAND_FLAG_UNEXPECTED);
	hlpr.cb=cb;
	hlpr.ptr=ptr;

	get_ssh_channel_expire_shared(channel, &expire, 20); /* 20 seconds expire ... is this a good choice ??? */

	while (((channel->flags & SSH_CHANNEL_FLAG_NODATA)==0) || (channel->queue.header.count>0)) {

	    payload=get_ssh_payload(&channel->queue, &expire, _cb_select_payload, _cb_break, _cb_error, (void *) &hlpr);

	    if (payload) {

		if (payload->type==SSH_MSG_CHANNEL_DATA) {

		    /* regular output like output of a command */
		    (* cb)(channel, &payload, SSH_CHANNEL_START_COMMAND_FLAG_DATA, 0, ptr);
		    result++;

		} else if (payload->type==SSH_MSG_CHANNEL_EXTENDED_DATA) {

		    /* additional output like errors (for example path to file to exec does not exist) */
		    (* cb)(channel, &payload, (SSH_CHANNEL_START_COMMAND_FLAG_DATA | SSH_CHANNEL_START_COMMAND_FLAG_ERROR), 0, ptr);
		    result++;

		} else {

		    /* unexpected output like requests */
		    (* cb)(channel, &payload, SSH_CHANNEL_START_COMMAND_FLAG_UNEXPECTED, 0, ptr);

		}

		if (payload) free_payload(&payload);

	    } else if (channel->flags & SSH_CHANNEL_FLAG_NODATA) {

                break;

            }

	}

    }

    out:
    return result;
}

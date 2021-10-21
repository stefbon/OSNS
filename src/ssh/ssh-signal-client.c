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

#include "log.h"
#include "main.h"
#include "misc.h"
#include "commonsignal.h"
#include "interface.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh-receive.h"
#include "ssh-connections.h"
#include "ssh-extensions.h"

#define REMOTE_COMMAND_DIRECTORY_DEFAULT		"/usr/lib/osns/"
#define REMOTE_COMMAND_DIRECTORY_TMP			"/tmp/"
#define REMOTE_COMMAND_GET_SERVERNAME			"getservername"
#define REMOTE_COMMAND_ENUM_SERVICES			"enumservices"
#define REMOTE_COMMAND_GET_SERVICE			"getservice"
#define REMOTE_COMMAND_SYSTEM_GETENTS			"systemgetents"
#define REMOTE_COMMAND_MAXLEN				256

static int custom_strncmp(const char *s, char *t)
{
    unsigned int len=strlen(t);

    if (strlen(s)>=len) return memcmp(s, t, len);
    return -1;
}

static unsigned int get_ssh2remote_command(char *command, unsigned int size, const char *what, struct ctx_option_s *o, const char *how)
{
    char *prefix=REMOTE_COMMAND_DIRECTORY_DEFAULT;

    if (command) {

	if (strcmp(what, "info:servername:")==0) {

	    return snprintf(command, size, "%s%s", prefix, REMOTE_COMMAND_GET_SERVERNAME);

	} else if (strcmp(what, "info:hostname:")==0) {

	    return snprintf(command, size, "echo $HOSTNAME");

	} else if (strcmp(what, "info:fqdn:")==0) {

	    return snprintf(command, size, "hostname --fqdn");

	} else if (strcmp(what, "info:enumservices:")==0) {

	    return snprintf(command, size, "%s%s", prefix, REMOTE_COMMAND_ENUM_SERVICES);

	} else if (strcmp(what, "info:service:")==0) {

	    return (o->type==_CTX_OPTION_TYPE_PCHAR) ? snprintf(command, size, "%s%s %s", prefix, REMOTE_COMMAND_GET_SERVICE, o->value.name) : -1;

	} else if (strcmp(what, "info:getentuser:")==0) {

	    return snprintf(command, size, "getent passwd $USER");

	}  else if (strcmp(what, "info:system.getents:")==0) {

	    return snprintf(command, size, "%s%s", prefix, REMOTE_COMMAND_SYSTEM_GETENTS);

	} else if (strcmp(what, "info:username:")==0) {

	    return snprintf(command, size, "echo $USER");

	} else if (strcmp(what, "info:getentgroup:")==0) {

	    return snprintf(command, size, "getent group $(id -g)");

	} else if (strcmp(what, "info:groupname:")==0) {

	    return snprintf(command, size, "id -gn)");

	} else if (strcmp(what, "info:remotehome:")==0) {

	    return snprintf(command, size, "echo $HOME");

	} else if (strcmp(what, "info:remotetime:")==0) {

	    return snprintf(command, size, "echo $(date +%s)", "%s.%N");

	}

	return 0;

    }

    return strlen(prefix) + REMOTE_COMMAND_MAXLEN;

}

/* get information from server through running a command */

static int _signal_ssh2remote_channel(struct ssh_session_s *session, const char *what, struct ctx_option_s *option, const char *how)
{
    struct ssh_channel_s *channel=NULL;
    struct channel_table_s *table=&session->channel_table;
    unsigned int size=get_ssh2remote_command(NULL, 0, what, option, how) + 2;
    char command[size+1];
    int result=-1;
    uint32_t seq;

    memset(command, 0, size+1);
    size=get_ssh2remote_command(command, size, what, option, how);
    logoutput("_signal_ssh2remote_channel: what %s how %s command %s", what, how, command);

    if (strcmp(how, "exec")==0) {
	struct ssh_string_s *exec=NULL;

	channel=create_channel(session, session->connections.main, _CHANNEL_TYPE_SESSION);
	if (channel==NULL) return -1;
	channel->name=_CHANNEL_NAME_SESSION;
	channel->target.session.type=_CHANNEL_SESSION_TYPE_EXEC;
	channel->target.session.name=_CHANNEL_SESSION_NAME_EXEC;
	exec=&channel->target.session.use.exec.command;
	create_ssh_string(&exec, strlen(command), command, SSH_STRING_FLAG_ALLOC);
	if (add_channel(channel, CHANNEL_FLAG_OPEN)==-1) goto free;

	result=send_channel_start_command_message(channel, 1, &seq);

    } else if (strcmp(how, "shell")==0) {

	/* commands have to end with newline&linefeed when in shell */
	command[size]=13;
	command[size+1]=10;
	size+=2;

	if (table->shell==NULL) {

	    if (table->flags & CHANNELS_TABLE_FLAG_SHELL) goto free;;
	    add_shell_channel(session);

	    if (table->shell==NULL) {

		table->flags |= CHANNELS_TABLE_FLAG_SHELL;
		return -2;

	    }

	    channel=table->shell;

	}

	result=send_channel_data_message(channel, command, size, &seq);

    }

    if (result>0) {
	struct timespec expire;
	struct ssh_payload_s *payload=NULL;
	unsigned int error=0;

	get_channel_expire_init(channel, &expire);

	getpayload:

	payload=get_ssh_payload_channel(channel, &expire, &seq, &error);
	if (payload==NULL) {

	    /**/
	    logoutput("_signal_ssh2remote_channel: no payload received");

	} else if (payload->type==SSH_MSG_CHANNEL_DATA || payload->type==SSH_MSG_CHANNEL_EXTENDED_DATA) {
	    char *buffer=(char *) payload;
	    unsigned char flags=payload->flags & SSH_PAYLOAD_FLAG_ERROR;

	    size=payload->len;
	    option->type=_CTX_OPTION_TYPE_BUFFER;
	    option->flags=_CTX_OPTION_FLAG_ALLOC;
	    option->flags|=(payload->type==SSH_MSG_CHANNEL_EXTENDED_DATA) ? _CTX_OPTION_FLAG_ERROR : 0;
	    option->value.buffer.ptr=memmove(buffer, payload->buffer, payload->len);
	    option->value.buffer.size=size;
	    option->value.buffer.len=size;
	    result=(int) size;

	    logoutput("_signal_ssh2remote_channel: %sreply %.*s", ((option->flags & _CTX_OPTION_FLAG_ERROR) ? "error " : ""), size, option->value.buffer.ptr);
	    payload=NULL;

	} else {

	    free_payload(&payload);
	    goto getpayload;

	}

	if (payload) free_payload(&payload);

    }

    if (strcmp(how, "exec")==0) remove_channel(channel, CHANNEL_FLAG_CLIENT_CLOSE | CHANNEL_FLAG_SERVER_CLOSE);

    free:

    if (strcmp(how, "exec")==0) {

	clear_ssh_string(&channel->target.session.use.exec.command);
	free_ssh_channel(&channel);

    }

    return result;
}

static int _signal_ssh2remote_exec_channel(struct ssh_session_s *session, const char *what, struct ctx_option_s *option)
{
    return _signal_ssh2remote_channel(session, what, option, "exec");
}

static int _signal_ssh2remote_shell_channel(struct ssh_session_s *session, const char *what, struct ctx_option_s *option)
{
    return _signal_ssh2remote_channel(session, what, option, "shell");
}

static int select_payload_request_reply(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    return (payload->type==SSH_MSG_REQUEST_SUCCESS || payload->type==SSH_MSG_REQUEST_FAILURE) ? 0 : -1;
}

/*
    custom SSH_MSG_GLOBAL_REQUEST to get info of server

    byte		SSH_MSG_GLOBAL_REQUEST
    string		"info-command@osns.net"
    byte		want reply
    string		what
    string		name (optional)

    NOTE: the sending and receiving is protected by a flag (SSH_CONNECTION_FLAG_GLOBAL_REQUEST) per connection
    different GLOBAL_REQUEST 's at the same time may lead to unexpected behaviour (replies got mixed)
*/

static void process_info_command_cb(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    if (payload->type==SSH_MSG_REQUEST_SUCCESS) {

	if (payload->len>5) {
	    struct ctx_option_s *option=(struct ctx_option_s *) ptr;
	    unsigned int size=get_uint32(&payload->buffer[1]);

	    logoutput("process_info_command_cb: received message (size=%i)", size);

	    /* reply is in string */

	    option->type=_CTX_OPTION_TYPE_BUFFER;
	    option->value.buffer.ptr=isolate_payload_buffer(&payload, 5, size);
	    option->value.buffer.size=size;
	    option->value.buffer.len=size;

	} else {

	    logoutput("process_info_command_cb: received message not big enough");

	}

    } else if (payload->type==SSH_MSG_REQUEST_FAILURE) {

	logoutput("process_info_command_cb: request not supported");

    }

}

static int _signal_ssh2remote_global_request(struct ssh_session_s *session, const char *what, struct ctx_option_s *option)
{
    struct ssh_connections_s *connections=&session->connections;
    struct ssh_connection_s *connection=connections->main;
    int result=-1;
    unsigned int len=strlen(what);
    char buffer[4 + len];

    store_uint32(buffer, len);
    memcpy(&buffer[4], what, len);

    if (process_global_request_message(connection, "info-command@osns.net", 0, buffer, len+4, process_info_command_cb, (void *) option)==0) {

	logoutput("signal_ssh2remote_global_request: request not supported");
	result=0;

    } else {

	logoutput("signal_ssh2remote_global_request: request not supported");

    }

    return result;
}

static int _signal_ssh2remote(struct ssh_session_s *session, const char *what, struct ctx_option_s *option)
{
    int result=-1;
    struct ssh_session_ctx_s *context=&session->context;

    if (context->flags & SSH_CONTEXT_FLAG_SSH2REMOTE_GLOBAL_REQUEST) {

	/* prefer via global request */

	result=_signal_ssh2remote_global_request(session, what, option);
	if (result>-2) return result;

    }

    if (session->context.flags & SSH_CONTEXT_FLAG_SSH2REMOTE_CHANNEL_SHELL) {

	result=_signal_ssh2remote_shell_channel(session, what, option);
	if (result>-2) return result;
	session->context.flags -= SSH_CONTEXT_FLAG_SSH2REMOTE_CHANNEL_SHELL;

    }

    return _signal_ssh2remote_exec_channel(session, what, option);

}

static int _signal_ssh2ctx_default(struct ssh_session_s *session, const char *what, struct ctx_option_s *o)
{
    return 0;
}

static int _signal_ctx2ssh(void **p_ptr, const char *what, struct ctx_option_s *option)
{
    void *ptr=*p_ptr;
    struct ssh_session_s *session=(struct ssh_session_s *) ptr;

    if (session==NULL) return -1;
    logoutput("signal_ctx2ssh: %s", what);

    if (custom_strncmp(what, "command:")==0) {
	unsigned pos=8;

	if (custom_strncmp(&what[pos], "disconnect:")==0 || custom_strncmp(&what[pos], "close:")==0) {

	    /* what to lock and check here */

	    signal_lock(session->connections.signal);

	    if (session->connections.flags & SSH_CONNECTIONS_FLAG_DISCONNECT) {

		signal_unlock(session->connections.signal);
		return -1;

	    }

	    session->connections.flags |= SSH_CONNECTIONS_FLAG_DISCONNECTING;
	    signal_unlock(session->connections.signal);

	    /* close all channels*/

	    _close_ssh_session_channels(session, "close");
	    _close_ssh_session_connections(session, "close");

	    signal_lock(session->connections.signal);
	    session->connections.flags |= SSH_CONNECTIONS_FLAG_DISCONNECTED;
	    signal_unlock(session->connections.signal);

	} else if (custom_strncmp(&what[pos], "free:")==0 || custom_strncmp(&what[pos], "clear:")==0) {

	    signal_lock(session->connections.signal);

	    if (session->connections.flags & SSH_CONNECTIONS_FLAG_CLEAR) {

		signal_unlock(session->connections.signal);
		return -1;

	    }

	    session->connections.flags |= SSH_CONNECTIONS_FLAG_CLEARING;
	    signal_unlock(session->connections.signal);

	    /* close and remove all channels*/

	    _close_ssh_session_channels(session, "remove");
	    _close_ssh_session_connections(session, "remove");

	    signal_lock(session->connections.signal);
	    session->connections.flags |= SSH_CONNECTIONS_FLAG_CLEARED;
	    signal_unlock(session->connections.signal);

	    _free_ssh_session(p_ptr);

	}

    } else if (custom_strncmp(what, "info:")==0) {
	unsigned pos=5;
	struct ssh_session_ctx_s *context=&session->context;

	if (custom_strncmp(&what[pos], "enumservices:")==0 || custom_strncmp(&what[pos], "system.getents:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "service:")==0) {

	    if (!(option->type==_CTX_OPTION_TYPE_PCHAR) || option->value.name==NULL) {

		pos += 8;

		if (strlen(&what[pos])>0) {
		    char *sep=strchr(&what[pos], ':');

		    if (sep) *sep='\0';

		    option->type=_CTX_OPTION_TYPE_PCHAR;
		    option->value.name=&what[pos];
		    return (* context->signal_ssh2remote)(session, what, option);

		}

	    } else {

		return (* context->signal_ssh2remote)(session, what, option);

	    }

	} else if (custom_strncmp(&what[pos], "username:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "getentuser:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "groupname:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "getentgroup:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "remotehome:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "remotetime:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "hostname:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "fqdn:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else if (custom_strncmp(&what[pos], "servername:")==0) {

	    return (* context->signal_ssh2remote)(session, what, option);

	} else {

	    logoutput("signal_ctx2ssh: command %s not reckognized", &what[pos]);

	}

    } else {

	logoutput("signal_ctx2ssh: %s not reckognized", what);

    }

    return 0;

}

void init_ssh_session_signals_client(struct ssh_session_ctx_s *context)
{
    context->signal_ctx2ssh=_signal_ctx2ssh;
    context->signal_ssh2ctx=_signal_ssh2ctx_default;
    context->signal_ssh2remote=_signal_ssh2remote;
}

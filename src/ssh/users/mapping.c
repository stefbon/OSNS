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

#include <pwd.h>
#include <grp.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"

#include "ssh-common.h"
#include "mapping.h"

/*
    get the uid and gid of the unknown user and group
    uid and gid of the remote host which are not reckognized
    (== not the connecting user/group, not root and not nobody...)
    get this uid/gid
*/

static void get_local_unknown_user(struct ssh_session_s *session)
{
    struct ssh_hostinfo_s *hostinfo=&session->hostinfo;
    struct net_idmapping_s *mapping=&hostinfo->mapping;
    struct passwd *pwd=NULL;
    char *user=NULL;
    struct io_option_s option;

    mapping->unknown_uid=(uid_t) -1;
    mapping->unknown_gid=(gid_t) -1;

    logoutput("get_local_unknown_user: try option:net.usermapping.user-unknown from context");

    /* look for option for user unknown */

    init_io_option(&option, 0);
    if ((* session->context.signal_ssh2ctx)(session, "option:net.usermapping.user-unknown", &option, INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION)>=0) {

	if (option.type==_IO_OPTION_TYPE_PCHAR) user=option.value.name;

    }

    pwd=(user ? getpwnam(user) : NULL);
    if (pwd) goto found;

    logoutput("get_local_unknown_user: try option:net.usermapping.user-nobody from context");

    /* look for option for user nobody */

    user=NULL;
    init_io_option(&option, 0);
    if ((* session->context.signal_ssh2ctx)(session, "option:net.usermapping.user-nobody", &option, INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION)>=0) {

	if (option.type==_IO_OPTION_TYPE_PCHAR) user=option.value.name;

    }

    pwd=((user) ? getpwnam(user) : NULL);
    if (pwd) goto found;

    logoutput("get_local_unknown_user: try user unknown");

    /* try user "unknown" */

    pwd=getpwnam("unknown");
    if (pwd) goto found;

    logoutput("get_local_unknown_user: try user nobody");

    /* try user "nobody" */

    pwd=getpwnam("nobody");
    if (pwd) goto found;
    logoutput("get_local_unknown_user: no user found");
    return;

    found:

    logoutput("get_local_unknown_user: user %i:%s", pwd->pw_uid, pwd->pw_name);
    mapping->unknown_uid=pwd->pw_uid;
    mapping->unknown_gid=pwd->pw_gid;

}

static int get_buffer_option(struct ssh_session_s *session, const char *how, const char *what, struct io_option_s *option)
{
    int result=-1;
    int (* signal_cb)(struct ssh_session_s *s, const char *what, struct io_option_s *o, unsigned int type);

    /* get the remote user properties and store in fields */

    if (strcmp(how, "remote")==0) {

	signal_cb=session->context.signal_ssh2remote;

    } else if (strcmp(how, "context")==0) {

	signal_cb=session->context.signal_ssh2ctx;

    } else {

	logoutput("get_remote_buffer_option: invalid how parameter %s", how);
	goto out;

    }

    if ((* signal_cb)(session, what, option, INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION)>=0) {

	/* reply is of command getent passwd $USER */

	if (option->type==_IO_OPTION_TYPE_BUFFER) {

	    logoutput("get_remote_buffer_option: received reply buffer: len %i", option->value.buffer.len);
	    result=0;

	} else {

	    logoutput("get_remote_buffer_option: reply from remote not a buffer");

	}

    } else {

	logoutput("get_remote_buffer_option: failed to signal remote");

    }

    out:

    logoutput_debug("get_remote_buffer_option: result %i", result);
    return result;
}

static int get_remote_identity_getents(struct ssh_session_s *session)
{
    struct net_idmapping_s *mapping=&session->hostinfo.mapping;
    struct getent_fields_s *getent=NULL;
    struct io_option_s option;
    int result=0;

    logoutput_debug("get_remote_identity_getents: A");

    init_io_option(&option, 0);

    if (get_buffer_option(session, "remote", "info:getentuser:", &option)==0) {
	struct ssh_string_s *tmp=&mapping->getent_su;

	if (option.type==_IO_OPTION_TYPE_BUFFER) {

	    tmp->ptr=option.value.buffer.ptr;
	    tmp->len=option.value.buffer.len;

	    option.value.buffer.ptr=NULL;
	    option.value.buffer.len=0;
	    option.value.buffer.size=0;

	}

	logoutput("get_remote_identity_getents: remote user getent %.*s", tmp->len, tmp->ptr);

    } else {

	logoutput("get_remote_identity_getents: not able to get getent of remote user");
	result=-1;

    }

    getent=&mapping->su;
    getent->flags |= GETENT_FLAG_USER;

    if (get_getent_fields(&mapping->getent_su, getent)==0) {

	logoutput("get_remote_identity_getents: user %i - %s:%s %s", getent->type.user.uid, getent->name, getent->type.user.fullname, getent->type.user.home);

    } else {

	logoutput("get_remote_identity_getents: not able to parse getent of remote user");
	result=-1;

    }

    (* option.free)(&option);
    init_io_option(&option, 0);

    if (get_buffer_option(session, "remote", "info:getentgroup:", &option)==0) {
	struct ssh_string_s *tmp=&mapping->getent_sg;

	if (option.type==_IO_OPTION_TYPE_BUFFER) {

	    tmp->ptr=option.value.buffer.ptr;
	    tmp->len=option.value.buffer.len;

	    option.value.buffer.ptr=NULL;
	    option.value.buffer.len=0;
	    option.value.buffer.size=0;

	}

	logoutput("get_remote_identity_getents: remote group getent %.*s", tmp->len, tmp->ptr);

    } else {

	logoutput("get_remote_identity_getents: not able to get getent of remote user");
	result=-1;

    }

    (* option.free)(&option);
    getent=&mapping->sg;
    getent->flags |= GETENT_FLAG_GROUP;

    if (get_getent_fields(&mapping->getent_sg, getent)==0) {

	logoutput("get_remote_identity_getents: group %i - %s", getent->type.group.gid, getent->name);

    } else {

	logoutput("get_remote_identity_getents: not able to parse getent of remote user");
	result=-1;

    }

    return result;

}

static int get_system_getents_file(struct net_idmapping_s *mapping, const char *target, struct ssh_string_s *file)
{
    struct ssh_session_s *session=(struct ssh_session_s *)((char *) mapping - offsetof(struct ssh_session_s, hostinfo.mapping));
    int result=-1;

    pthread_mutex_lock(mapping->mutex);

    if ((mapping->flags & NET_IDMAPPING_FLAG_COMPLETE)==0) {

	logoutput_warning("get_system_getents_file: usermapping not done");
	pthread_mutex_unlock(mapping->mutex);
	return 0;

    }

    if (strcmp(target, "remote")==0) {
	struct io_option_s option;

	init_io_option(&option, 0);

	if (get_buffer_option(session, "remote", "info:system.getents:", &option)>=0) {

	    if (option.type==_IO_OPTION_TYPE_BUFFER) {

		file->ptr=option.value.buffer.ptr;
		file->len=option.value.buffer.len;

		option.value.buffer.ptr=NULL;
		option.value.buffer.len=0;
		option.value.buffer.size=0;
		result=0;

	    }

	}

    // } else if (strcmp(target, "local")==0) {

	// if (get_local_option(session, "info:system.getents:", file)>=0) result=0;

    }

    pthread_mutex_unlock(mapping->mutex);

    if (result==0) {

	logoutput("get_system_getents_file: received %s file %.*s", target, file->len, file->ptr);

    } else {

	logoutput_warning("get_system_getents_file: unable to get %s file", target);

    }

}

static int setup_net_usermapping_done(struct net_idmapping_s *mapping, unsigned int flags)
{
    return 0;
}

static int setup_net_usermapping(struct net_idmapping_s *mapping, unsigned int flags)
{
    struct ssh_session_s *session=(struct ssh_session_s *)((char *) mapping - offsetof(struct ssh_session_s, hostinfo.mapping));
    struct io_option_s option;
    struct ssh_string_s reply=SSH_STRING_INIT;

    if ((flags & (NET_IDMAPPING_FLAG_MAPBYID | NET_IDMAPPING_FLAG_MAPBYNAME))==0) {

	logoutput_warning("get_net_usermapping: flags %i do not provide the mapping by name or id", flags);
	return -1;

    }

    pthread_mutex_lock(mapping->mutex);

    if (mapping->flags & (NET_IDMAPPING_FLAG_STARTED | NET_IDMAPPING_FLAG_COMPLETE)) {

	logoutput_warning("get_net_usermapping: usermapping already %s", ((mapping->flags & NET_IDMAPPING_FLAG_STARTED) ? "started" : "done"));
	mapping->setup=setup_net_usermapping_done;
	pthread_mutex_unlock(mapping->mutex);
	return 0;

    }

    mapping->flags |= NET_IDMAPPING_FLAG_STARTED;
    pthread_mutex_unlock(mapping->mutex);

    get_local_unknown_user(session);

    if (get_remote_identity_getents(session)==0) {

	logoutput("setup_net_usermapping: remote user and group set");

    } else {

	logoutput("setup_net_usermapping: failed to get remote user and group");
	goto out;

    }

    init_io_option(&option, 0);

    if (get_buffer_option(session, "context", "option:net.usermapping.type", &option)==0) {

	if (option.type==_IO_OPTION_TYPE_BUFFER) {

	    reply.ptr=option.value.buffer.ptr;
	    reply.len=option.value.buffer.len;
	    option.value.buffer.ptr=NULL;
	    option.value.buffer.len=0;
	    option.value.buffer.size=0;

	}

	/* TODO: add the following modus of handling user- and groupnames:
	    - strict (default) versus nonstrict, where to get this??
	*/

	if ((compare_ssh_string(&reply, 'c', "none")==0) | (compare_ssh_string(&reply, 'c', "shared")==0)) {

	    /* no mapping or translation required, uid and gid are shared via ldap/ad for example  */

	    mapping->flags |= NET_IDMAPPING_FLAG_SHARED;

	} else if ((compare_ssh_string(&reply, 'c', "map")==0) | (compare_ssh_string(&reply, 'c', "nonshared")==0)) {

	    /* simple mapping is used like:
		local user 	<-> remote user
		root		<-> root
		nobody		<-> nobody
		everything else mapped to the unknown user */

	    mapping->flags &= ~NET_IDMAPPING_FLAG_SHARED;

	} else {

	    logoutput_warning("setup_net_usermapping: option net.usermapping.type value %.*s not reckognized", reply.len, reply.ptr);

	}

    } else {

	logoutput_warning("setup_net_usermapping: option net.usermapping.type not reckognized by context");

    }

    (* option.free)(&option);
    clear_ssh_string(&reply);

    mapping->flags |= (flags & (NET_IDMAPPING_FLAG_MAPBYID | NET_IDMAPPING_FLAG_MAPBYNAME | NET_IDMAPPING_FLAG_CLIENT | NET_IDMAPPING_FLAG_SERVER));

    out:

    set_net_entity_map_func(mapping, flags);
    mapping->setup=setup_net_usermapping_done;

    pthread_mutex_lock(mapping->mutex);
    mapping->flags &= ~NET_IDMAPPING_FLAG_STARTED;
    mapping->flags |= NET_IDMAPPING_FLAG_COMPLETE;
    pthread_mutex_unlock(mapping->mutex);

    return 1;

}

void init_ssh_usermapping(struct ssh_session_s *session, struct passwd *pwd)
{
    struct net_idmapping_s *mapping=&session->hostinfo.mapping;
    init_net_idmapping(mapping, pwd);
    mapping->setup=setup_net_usermapping;

}

void free_ssh_usermapping(struct ssh_session_s *session)
{
    free_net_idmapping(&session->hostinfo.mapping);
}

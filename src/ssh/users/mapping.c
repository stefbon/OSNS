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
#include "libosns-users.h"

#include "ssh-common.h"
#include "ssh-channel.h"
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

    /* look for option for user unknown */

    init_io_option(&option, 0);
    if ((* session->context.signal_ssh2ctx)(session, "option:net.usermapping.user-unknown", &option, INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION)>=0) {

	if (option.type==_IO_OPTION_TYPE_PCHAR) user=option.value.name;

    }

    pwd=(user ? getpwnam(user) : NULL);
    if (pwd) goto found;

    /* look for option for user nobody */

    user=NULL;
    init_io_option(&option, 0);
    if ((* session->context.signal_ssh2ctx)(session, "option:net.usermapping.user-nobody", &option, INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION)>=0) {

	if (option.type==_IO_OPTION_TYPE_PCHAR) user=option.value.name;

    }

    pwd=((user) ? getpwnam(user) : NULL);
    if (pwd) goto found;

    /* try user "unknown" */

    pwd=getpwnam("unknown");
    if (pwd) goto found;

    /* try user "nobody" */

    pwd=getpwnam("nobody");
    if (pwd) goto found;
    return;

    found:

    logoutput_debug("get_local_unknown_user: user %i:%s", pwd->pw_uid, pwd->pw_name);
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

static unsigned int scan_str2array(char *data, unsigned int len, char **ptr, unsigned int count)
{
    unsigned int tmp=0;
    unsigned int ctr=0;
    char *pos=data;
    char *sep=NULL;

    assignarray:

    sep=memchr(pos, ':', (unsigned int)(data + len - pos));
    if (ctr<count) ptr[ctr]=pos;
    ctr++;

    if (sep) {

	if (count>0) *sep='\0';
	pos=sep+1;
	if (pos < (char *)(data+len)) goto assignarray;

    }

    return ctr;
}

static void add_user2cache(struct ssh_session_s *session, char *buffer, unsigned int len)
{
    unsigned int count=scan_str2array(buffer, len, NULL, 0);

	/*
	    buffer is like:

	    %USER%:x:%UID%:%GID%:%NAME%:%HOME:%SHELL%
	    so at least 3 fields are required: username and uid 

	*/

    if (count>=3 && count<=20) {
	char *aptr[count];
	struct net_idmapping_s *mapping=&session->hostinfo.mapping;
	struct net_entity_s ent;
	unsigned int errcode=0;
	struct net_userscache_s *cache=mapping->cache;

	count=scan_str2array(buffer, len, aptr, count);
	memset(&ent, 0, sizeof(struct net_entity_s));

	/* 20220907:
	    only name is supported now, not domain */

	ent.flags=NET_ENTITY_FLAG_USER;
	ent.net.name.ptr=aptr[0];
	ent.net.name.len=strlen(aptr[0]);
	ent.net.id=atoi(aptr[2]);
	ent.localid=mapping->unknown_uid; /* this is overwritten with a successfull lookup */

	lookup_user_byname_system(mapping, &ent, &errcode);

	if ((* cache->add_net2local_map)(cache, &ent)==0) {

	    logoutput_debug("add_user2cache: added uid %u - %.*s to cache (mapped to %u)", ent.net.id, ent.net.name.len, ent.net.name.ptr, ent.localid);

	} else {

	    logoutput_debug("add_user2cache: unable to add uid %u - %.*s to cache", ent.net.id, ent.net.name.len, ent.net.name.ptr);

	}

    }

}

static void add_group2cache(struct ssh_session_s *session, char *buffer, unsigned int len)
{
    unsigned int count=scan_str2array(buffer, len, NULL, 0);

	/*
	    buffer is like:

	    %GROUP%:x:%GID%:%MEMBERS%
	    so at least 3 fields are required (name and gid)

	*/


    if (count>=3 && count<=20) {
	char *aptr[count];
	struct net_idmapping_s *mapping=&session->hostinfo.mapping;
	struct net_entity_s ent;
	unsigned int errcode=0;
	struct net_userscache_s *cache=mapping->cache;

	count=scan_str2array(buffer, len, aptr, count);
	memset(&ent, 0, sizeof(struct net_entity_s));

	/* 20220907:
	    only name is supported now, not domain */

	ent.flags=NET_ENTITY_FLAG_GROUP;
	ent.net.name.ptr=aptr[0];
	ent.net.name.len=strlen(aptr[0]);
	ent.net.id=atoi(aptr[2]);
	ent.localid=mapping->unknown_gid; /* this is overwritten with a successfull lookup */

	lookup_group_byname_system(mapping, &ent, &errcode);

	if ((* cache->add_net2local_map)(mapping->cache, &ent)==0) {

	    logoutput_debug("add_user2cache: added gid %u - %.*s to cache (mapped to %u)", ent.net.id, ent.net.name.len, ent.net.name.ptr, ent.localid);

	} else {

	    logoutput_debug("add_user2cache: unable to add gid %u - %.*s to cache", ent.net.id, ent.net.name.len, ent.net.name.ptr);

	}

    }

}

static void _cb_enumusers_exec(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload, unsigned int flags, unsigned int errcode, void *ptr)
{
    struct ssh_session_s *session=channel->session;
    struct _cb_exec_hlpr_s *hlpr=(struct _cb_exec_hlpr_s *) ptr;
    struct ssh_payload_s *payload=*p_payload;

    if (payload) {
	char *buffer=(char *) payload;
	unsigned int len=payload->len; /* keep since payload->len will be overwritten */

	memmove(buffer, payload->buffer, len);

	if ((flags & SSH_CHANNEL_EXEC_FLAG_ERROR)==0) {

	    logoutput_debug("_cb_enumusers_exec: recv %.*s", len, buffer);

	    /* buffer is like:
		0001:%USER%:x:%UID%:%GID%:%NAME%:%HOME%:%SHELL%
		0002:%GROUP%:x:%GID%:%MEMBERS%
	    */

	    if (len>5) {

		if (memcmp(buffer, "0001:", 5)==0) {

		    add_user2cache(session, &buffer[5], len-5);

		} else if (memcmp(buffer, "0002:", 5)==0) {

		    add_group2cache(session, &buffer[5], len-5);

		} else {

		    logoutput_debug("_cb_enumusers_exec: string starting with code %.*s ... not reckognized", 4, buffer);

		}

	    }

	} else {

	    logoutput_debug("_cb_enumusers_exec: errcode %u %.*s", errcode, len, buffer);

	}

    }

}

static int get_remote_usersinfo(struct net_idmapping_s *mapping)
{
    struct ssh_session_s *session=(struct ssh_session_s *)((char *) mapping - offsetof(struct ssh_session_s, hostinfo.mapping));
    unsigned int len=get_ssh2remote_command(NULL, 0, "info:enumusers:", NULL);
    char command[len+1];

    memset(command, 0, len+1);
    len=get_ssh2remote_command(command, len+1, "info:enumusers:", NULL);
    logoutput_debug("get_remote_usersinfo: command %s");

    return exec_remote_command(session, command, _cb_enumusers_exec, NULL);

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

	if ((compare_ssh_string(&reply, 'c', "none")==0) || (compare_ssh_string(&reply, 'c', "shared")==0)) {

	    /* no mapping or translation required, uid and gid are shared via ldap/ad for example  */

	    mapping->flags |= NET_IDMAPPING_FLAG_SHARED;

	} else if ((compare_ssh_string(&reply, 'c', "map")==0) || (compare_ssh_string(&reply, 'c', "nonshared")==0)) {

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
	mapping->flags &= ~NET_IDMAPPING_FLAG_SHARED;

    }

    (* option.free)(&option);
    clear_ssh_string(&reply);

    if ((mapping->flags & NET_IDMAPPING_FLAG_SHARED)==0) {

	mapping->cache=create_net_userscache(mapping->flags);

	if (mapping->cache) {

	    int result=get_remote_usersinfo(mapping);
	    if (result>0) {

		logoutput_debug("setup_net_usermapping: enabling cache (rows read %u)", (unsigned int) result);
		mapping->flags |= NET_IDMAPPING_FLAG_CACHE;

	    } else {

		logoutput_debug("setup_net_usermapping: not enabling cache");

	    }

	}

    }

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

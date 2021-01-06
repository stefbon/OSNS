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
#include "fuse/network.h"
#include "fuse/sftp-fs.h"

#define _SFTP_NETWORK_NAME			"SFTP_Network"
#define _SFTP_HOME_MAP				"home"

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

static int get_service_info_prefix(struct ctx_option_s *option, char **prefix, char **uri)
{
    char *pos=option->value.buffer.ptr;
    int left=option->value.buffer.len;
    char *sep=memchr(pos, '|', left);
    if (sep==NULL) return -1;
    *prefix=pos;
    *sep='\0';
    sep++;
    if ((int) (sep - pos) >= left - 2) return 0;
    left -= (int)(sep - option->value.buffer.ptr);
    pos=sep;
    sep=memchr(pos, '|', left);
    if (sep==NULL) return 0;
    *uri=pos;
    *sep='\0';
    return 0;
}

static int add_sftp_context(struct service_context_s *parent_context, struct inode_s *inode, char *name, char *prefix, char *uri, struct interface_list_s *ailist)
{
    struct workspace_mount_s *workspace=parent_context->workspace;
    struct service_context_s *context=NULL;
    struct context_interface_s *interface=NULL;
    struct service_address_s service;
    int fd=-1;
    struct interface_list_s *ilist=NULL;
    unsigned int error=0;

    ilist=get_interface_ops(ailist, 0, _INTERFACE_TYPE_SFTP);
    if (ilist==NULL) {

	logoutput("add_sftp_context: interface not found");
	return -1;

    }

    context=create_service_context(workspace, parent_context, ilist, SERVICE_CTX_TYPE_FILESYSTEM, NULL);

    if (context==NULL) {

	logoutput("add_sftp_context: error allocating context");
	return -1;

    }

    memset(&service, 0, sizeof(struct service_address_s));
    service.type=_SERVICE_TYPE_SFTP_CLIENT;
    service.target.sftp.name=name;
    service.target.sftp.uri=uri;

    context->service.filesystem.inode=inode;
    set_context_filesystem_sftp(context, 0);
    interface=&context->interface;
    interface->signal_context=signal_sftp2ctx;

    logoutput("add_sftp_context: connect");

    fd=(* interface->connect)(workspace->user->pwd.pw_uid, interface, NULL, &service);

    if (fd==-1) {

	logoutput("add_sftp_context: error connecting sftp");
	return -1;

    }

    logoutput("add_sftp_context: start");

    if ((* interface->start)(interface, fd, NULL)==0) {
	struct simple_lock_s wlock3;
	struct directory_s *directory=get_directory(inode);

	if (wlock_directory(directory, &wlock3)==0) {
	    struct inode_link_s link;

	    use_service_root_fs(inode);
	    link.type=INODE_LINK_TYPE_CONTEXT;
	    link.link.ptr= (void *) context;
	    set_inode_link_directory(inode, &link);
	    inode->nlookup=1;
	    logoutput("add_sftp_context: added sftp context %s", name);
	    unlock_directory(directory, &wlock3);

	}

    } else {

	logoutput("add_sftp_context: unable to start sftp");
	use_virtual_fs(NULL, inode); /* entry may exist, but has not connection */
	return -1;

    }

    set_sftp_interface_prefix(interface, name, prefix);

    if (fs_options.network.share_icon & (_OPTIONS_NETWORK_ICON_OVERRULE)) {
	struct simple_lock_s wlock4;
	struct directory_s *directory=get_directory(inode);

	/* create a desktp entry only if it does not exist on the server/share */

	if (wlock_directory(directory, &wlock4)==0) {

	    create_desktopentry_file("/etc/fs-workspace/desktopentry.sharedmap", inode->alias, workspace);
	    unlock_directory(directory, &wlock4);

	}

    }

    return 0;

}


/* add a sftp shared map */

void add_shared_map_sftp(struct service_context_s *parent_context, struct inode_s *inode, char *name, struct interface_list_s *ailist)
{
    struct workspace_mount_s *workspace=parent_context->workspace;
    struct context_interface_s *interface=&parent_context->interface;
    struct entry_s *parent=inode->alias;
    struct entry_s *entry=NULL;
    struct directory_s *directory=NULL;
    struct name_s xname;
    unsigned int error=0;
    unsigned int len=strlen(name);
    struct ctx_option_s option;
    unsigned char done=0;

    logoutput("add_shared_map_sftp: name %s", name);

    /* get rid of nasty/unwanted characters in name: note the name will be used a name for an entry in the filesystem */

    replace_cntrl_char(name, len, REPLACE_CNTRL_FLAG_TEXT);
    if (skip_heading_spaces(name, len)>0) len=strlen(name);
    if (skip_trailing_spaces(name, len, SKIPSPACE_FLAG_REPLACEBYZERO)>0) len=strlen(name);

    /* what name to use for home directory */

    if (strcmp(name, "home")==0 && (fs_options.sftp.flags & _OPTIONS_SFTP_FLAG_HOME_USE_REMOTENAME)) {

	/* get the remote username from the ssh server */

	init_ctx_option(&option, _CTX_OPTION_TYPE_BUFFER);
	if ((* interface->signal_interface)(interface, "info:username:", &option)>=0) {

	    if (option.type==_CTX_OPTION_TYPE_BUFFER && option.value.buffer.ptr && option.value.buffer.len>0) {
		unsigned int size=option.value.buffer.len;
		char user[size + 1];
		struct simple_lock_s wlock1;

		memcpy(user, option.value.buffer.ptr, size);
		user[size]='\0';

		logoutput("add_shared_map_sftp: replacing home by remote username %s", user);

		replace_cntrl_char(user, size, REPLACE_CNTRL_FLAG_TEXT);
		if (skip_heading_spaces(user, size)>0) size=strlen(user);
		if (skip_trailing_spaces(user, size, SKIPSPACE_FLAG_REPLACEBYZERO)>0) size=strlen(user);

		xname.name=user;
		xname.len=strlen(user);
		calculate_nameindex(&xname);
		directory=get_directory(inode);

		if (wlock_directory(directory, &wlock1)==0) {

		    entry=create_network_map_entry(parent_context, directory, &xname, &error);

		    if (entry) {

			logoutput("add_shared_map_sftp: created shared map %s", user);
			parent=entry;

		    } else {

			logoutput("add_shared_map_sftp: failed to create map %s, error %i (%s)", user, error, strerror(error));

		    }

		    unlock_directory(directory, &wlock1);

		}

	    }

	    (* option.free)(&option);

	}

    }

    /* if no entry created (=home directory) do it now */

    if (entry==NULL) {
	struct simple_lock_s wlock2;

	xname.name=name;
	xname.len=strlen(name);
	calculate_nameindex(&xname);
	directory=get_directory(parent->inode);

	if (wlock_directory(directory, &wlock2)==0) {

	    directory=get_directory(parent->inode);
	    entry=create_network_map_entry(parent_context, directory, &xname, &error);
	    if (entry) logoutput("add_shared_map_sftp: created shared map %s", name);
	    unlock_directory(directory, &wlock2);

	}

    }

    /* entry created and no error (no EEXIST!) */

    if (entry==NULL || error>0) {

	/* TODO: when entry already exists (error==EEXIST) continue */

	if (entry && error==EEXIST) {

	    logoutput("add_shared_map_sftp: directory %s does already exist", name);
	    error=0;

	} else {

	    if (error==0) error=EIO;
	    logoutput("add_shared_map_sftp: error %i creating directory %s (%s)", error, name, strerror(error));

	}

	goto out;

    }

    inode=entry->inode;
    init_ctx_option(&option, _CTX_OPTION_TYPE_PCHAR);
    option.value.name=name;

    if ((* interface->signal_interface)(interface, "info:service:", &option)>=0 &&
	option.type==_CTX_OPTION_TYPE_BUFFER && option.value.buffer.ptr && option.value.buffer.len>0) {

	if ((option.flags & _CTX_OPTION_FLAG_ERROR)==0) {
	    char *prefix=NULL;
	    char *uri=NULL;

	    /* buffer looks like:

		/home/public|socket://run/fileserver/sock|
		(direct-streamlocal)

		or

		/home/sbon|
		(default sftp subsystem)

		or

		/home/joe|ssh://192.168.1.8:2222|
		(direct-tcpip)
	    */

	    if (get_service_info_prefix(&option, &prefix, &uri)==-1) {

		(* option.free)(&option);
		goto error;

	    }

	    if (add_sftp_context(parent_context, inode, name, prefix, uri, ailist)==0) {

		logoutput("add_shared_map_sftp: sftp context added");
		done=1;

	    } else {

		logoutput("add_shared_map_sftp: failed to add sftp context");

	    }

	}

	(* option.free)(&option);

    }

    if (done==0) {

	if (add_sftp_context(parent_context, inode, name, NULL, NULL, ailist)==0) {

	    logoutput("add_shared_map_sftp: sftp context added");

	} else {

	    logoutput("add_shared_map_sftp: failed to add sftp context");

	}

    }

    /* when dealing with backup start backup service */

    // if (strcmp(name, "backup")==0) {

	// start_backup_service(context);

    // } else if (strcmp(name, "backupscript")==0) {

	// start_backupscript_service(context);

    // }

    out:
    return;

    error:
    logoutput("add_shared_map_sftp: error");

}

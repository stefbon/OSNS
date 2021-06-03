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
#include <sys/syscall.h>
#include <sys/statfs.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "options.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "workspace/access.h"
#include "workspace/getattr.h"
#include "workspace/lookup.h"
#include "workspace/opendir.h"
#include "workspace/xattr.h"
#include "workspace/statfs.h"

#include "discover/discover.h"
#include "network.h"

#define UINT32_T_MAX		0xFFFFFFFF

static unsigned int _fs_browse_get_name(struct service_context_s *context, char *buffer, unsigned int len)
{
    struct discover_resource_s *resource=lookup_resource_id(context->service.browse.unique);
    unsigned int size=0;

    if (resource==NULL) {

	logoutput_debug("_fs_browse_get_name: no resource record found ...");

    } else if (resource->type==DISCOVER_RESOURCE_TYPE_NETWORK_GROUP) {
	char *name=resource->service.group.name;

	logoutput_debug("_fs_browse_get_name: resource network group %s found", name);

	size=strlen(name);

	if (buffer) {

	    if (size>len) size=len;
	    memcpy(buffer, name, size);

	}

    } else if (resource->type==DISCOVER_RESOURCE_TYPE_NETWORK_HOST) {
	char *name=NULL;

	switch (resource->service.host.lookupname.type) {

	    case LOOKUP_NAME_TYPE_CANONNAME:

		name=resource->service.host.lookupname.name.canonname;
		break;

	    case LOOKUP_NAME_TYPE_DNSNAME:

		name=resource->service.host.lookupname.name.dnsname;
		break;

	    case LOOKUP_NAME_TYPE_DISCOVERNAME:

		name=resource->service.host.lookupname.name.discovername;
		break;

	}

	if (name) {
	    char *sep=NULL;

	    logoutput_debug("_fs_browse_get_name: resource network host %s found", name);

	    size=strlen(name);
	    sep=memchr(name, '.', size);
	    if (sep) size=(unsigned int)(sep - name);

	    if (buffer) {

		if (size<len) len=size;
		memcpy(buffer, name, len);

	    }

	} else {

	    logoutput("_fs_browse_get_name: no name for resource network host found");

	}

    }

    return size;

}

static unsigned int _fs_workspace_get_name(struct service_context_s *context, char *buffer, unsigned int len)
{
    unsigned int size=0;

    if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {
	struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
	char *name=NULL;

	if (workspace->type==WORKSPACE_TYPE_DEVICES) {

	    name=(char *) "workspace:devices";

	} else if (workspace->type==WORKSPACE_TYPE_NETWORK) {

	    name=(char *) "workspace:network";

	} else if (workspace->type==WORKSPACE_TYPE_BACKUP) {

	    name=(char *) "workspace:backup";

	}

	if (name) {

	    size=strlen(name);

	    if (buffer) {

		if (size<len) len=size;
		memcpy(buffer, name, len);

	    }

	}


    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	if (context->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) {
	    unsigned int ctxflags = (context->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND);
	    char *name=NULL;

	    if (ctxflags & SERVICE_CTX_FLAG_SFTP) {

		name=get_network_name(SERVICE_CTX_FLAG_SFTP);

	    } else if (ctxflags & SERVICE_CTX_FLAG_NFS) {

		name=get_network_name(SERVICE_CTX_FLAG_NFS);

	    } else if (ctxflags & SERVICE_CTX_FLAG_SMB) {

		name=get_network_name(SERVICE_CTX_FLAG_SMB);

	    } else if (ctxflags & SERVICE_CTX_FLAG_WEBDAV) {

		name=get_network_name(SERVICE_CTX_FLAG_WEBDAV);

	    } else if (ctxflags & SERVICE_CTX_FLAG_ICS) {

		name=get_network_name(SERVICE_CTX_FLAG_ICS);

	    }

	    if (name) {

		logoutput("_fs_workspace_get_name: found name %s for %i:%i:%s", name, context->type, context->service.browse.type, context->name);

		size=strlen(name);

		if (buffer) {

		    if (size<len) len=size;
		    memcpy(buffer, name, len);

		}

	    } else {

		logoutput("_fs_workspace_get_name: no name found for %i:%i", context->type, context->service.browse.type);

	    }

	} else {

	    size=_fs_browse_get_name(context, buffer, len);

	}

    }

    logoutput("_fs_workspace_get_name: return size %i", size);

    return size;

}


/* this fs is attached to the rootinode of the network fuse mountpoint */

static struct service_fs_s workspace_fs = {

    .lookup_existing		= _fs_workspace_lookup_existing,
    .lookup_new			= _fs_workspace_lookup_new,

    .getattr			= _fs_workspace_getattr,
    .setattr			= _fs_workspace_setattr,

    .access			= _fs_workspace_access,
    .get_name			= _fs_workspace_get_name,

    .mkdir			= NULL,
    .mknod			= NULL,
    .symlink			= NULL,
    .symlink_validate		= NULL,
    .readlink			= NULL,

    .unlink			= NULL,
    .rmdir			= NULL,

    .create			= NULL,
    .open			= NULL,
    .read			= NULL,
    .write			= NULL,
    .fsync			= NULL,
    .flush			= NULL,
    .fgetattr			= NULL,
    .fsetattr			= NULL,
    .release			= NULL,

    .getlock			= NULL,
    .setlock			= NULL,
    .setlockw			= NULL,
    .flock			= NULL,

    .opendir			= _fs_workspace_opendir,
    .readdir			= _fs_workspace_readdir,
    .readdirplus		= _fs_workspace_readdirplus,
    .fsyncdir			= _fs_workspace_fsyncdir,
    .releasedir			= _fs_workspace_releasedir,

    .getxattr			= _fs_workspace_getxattr,
    .setxattr			= _fs_workspace_setxattr,
    .listxattr			= _fs_workspace_listxattr,
    .removexattr		= _fs_workspace_removexattr,

    .statfs			= _fs_workspace_statfs,

};

void set_context_filesystem_workspace(struct service_context_s *context)
{
    if (context->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	context->service.filesystem.fs=&workspace_fs;

    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	context->service.browse.fs=&workspace_fs;

    } else if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	context->service.workspace.fs=&workspace_fs;

    } else {

	logoutput_warning("set_context_filesystem_workspace: context if of type %i (not %i = filesystem)", context->type, SERVICE_CTX_TYPE_FILESYSTEM);

    }

}

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

#include "main.h"
#include "misc.h"
#include "log.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "smb/access.h"
#include "smb/getattr.h"
#include "smb/lookup.h"
#include "smb/opendir.h"
#include "smb/readlink.h"

static unsigned int _fs_smb_get_name(struct service_context_s *context, char *buffer, unsigned int len)
{
    struct context_interface_s *interface=&context->interface;
    char *name=interface->backend.smb_share.name;
    unsigned int size=0;

    if (name) {

	size=strlen(name);

	if (buffer) {

	    if (size<len) len=size;
	    memcpy(buffer, name, len);

	}

    }

    return size;

}

/* generic sftp fs */

static struct service_fs_s smb_fs = {

    .lookup_existing		= _fs_smb_lookup_existing,
    .lookup_new			= _fs_smb_lookup_new,

    .getattr			= _fs_smb_getattr,

    .access			= _fs_smb_access,
    .get_name			= _fs_smb_get_name,
    .readlink			= _fs_smb_readlink,

    .opendir			= _fs_smb_opendir,
    .readdir			= _fs_smb_readdir,
    .readdirplus		= _fs_smb_readdirplus,
    .fsyncdir			= _fs_smb_fsyncdir,
    .releasedir			= _fs_smb_releasedir,

};

static struct service_fs_s smb_fs_disconnected = {

    .lookup_existing		= _fs_smb_lookup_existing_disconnected,
    .lookup_new			= _fs_smb_lookup_new_disconnected,

    .getattr			= _fs_smb_getattr_disconnected,

    .access			= _fs_smb_access,
    .get_name			= _fs_smb_get_name,
    .readlink			= _fs_smb_readlink_disconnected,

    .opendir			= _fs_smb_opendir_disconnected,
    .readdir			= _fs_smb_readdir_disconnected,
    .readdirplus		= _fs_smb_readdirplus_disconnected,
    .fsyncdir			= _fs_smb_fsyncdir_disconnected,
    .releasedir			= _fs_smb_releasedir_disconnected,

};

void set_context_filesystem_smb(struct service_context_s *context, unsigned char disconnect)
{
    context->service.filesystem.fs=(disconnect) ? &smb_fs_disconnected : &smb_fs;
}

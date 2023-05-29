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
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "sftp/fstatat.h"
#include "sftp/getattr.h"
#include "sftp/lookup.h"
#include "sftp/lock.h"
#include "sftp/open.h"
#include "sftp/opendir.h"
#include "sftp/rm.h"
#include "sftp/mk.h"
#include "sftp/setattr.h"
#include "sftp/symlink.h"
#include "sftp/statfs.h"

#include "shared/access.h"
#include "shared/xattr.h"
#include "shared/name.h"

/* generic sftp fs */

static struct path_service_fs_s sftp_fs = {

    .get_name			= _fs_shared_get_name,

    .lookup			= _fs_sftp_lookup,
    .fstatat			= _fs_sftp_fstatat,

    .getattr			= _fs_sftp_getattr,
    .setattr			= _fs_sftp_setattr,

    .access			= _fs_shared_access,

    .mkdir			= _fs_sftp_mkdir,
    .mknod			= _fs_sftp_mknod,
    .symlink			= _fs_sftp_symlink,
    .symlink_validate		= _fs_sftp_symlink_validate,
    .readlink			= _fs_sftp_readlink,

    .unlink			= _fs_sftp_unlink,
    .rmdir			= _fs_sftp_rmdir,

    .open			= _fs_sftp_open,
    .opendir			= _fs_sftp_opendir,

    .getxattr			= _fs_shared_getxattr,
    .setxattr			= _fs_shared_setxattr,
    .listxattr			= _fs_shared_listxattr,
    .removexattr		= _fs_shared_removexattr,

    .statfs			= _fs_sftp_statfs,

};

/* initialize a sftp subsystem interface using sftp fs */

void set_context_filesystem_sftp(struct service_context_s *context)
{
    context->service.filesystem.fs=&sftp_fs;
}

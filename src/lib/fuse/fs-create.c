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

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "defaults.h"
#include "dentry.h"
#include "directory.h"
#include "opendir.h"
#include "openfile.h"
#include "request.h"
#include "fs-create.h"

#include "utils.h"
#include "utils-create.h"

extern struct fuse_config_s *get_fuse_interface_config(struct context_interface_s *i);

void _fs_common_cached_create(struct service_context_s *ctx, struct fuse_request_s *request, struct fuse_openfile_s *openfile)
{
    struct fuse_config_s *config=get_fuse_interface_config(request->interface);
    struct inode_s *inode=openfile->header.inode;
    struct fuse_entry_out eout;
    struct fuse_open_out oout;
    unsigned int size_eout=sizeof(struct fuse_entry_out);
    unsigned int size_oout=sizeof(struct fuse_open_out);
    struct system_timespec_s *attr_timeout=&config->attr_timeout;
    struct system_timespec_s *entry_timeout=&config->entry_timeout;
    char buffer[size_eout + size_oout];

    // inode->nlookup++;
    memset(&eout, 0, size_eout);
    eout.nodeid=inode->stat.sst_ino;
    eout.generation=0; /* todo: add a generation field to reuse existing inodes */
    eout.entry_valid=get_system_time_sec(entry_timeout);
    eout.entry_valid_nsec=get_system_time_nsec(entry_timeout);
    eout.attr_valid=get_system_time_sec(attr_timeout);
    eout.attr_valid_nsec=get_system_time_nsec(attr_timeout);
    fill_fuse_attr_system_stat(&eout.attr, &inode->stat);

    memset(&oout, 0, size_oout);
    oout.fh=(uint64_t) openfile;
    oout.open_flags=FOPEN_KEEP_CACHE;

    /* put entry_out and open_out in one buffer */

    memcpy(buffer, &eout, size_eout);
    memcpy(buffer+size_eout, &oout, size_oout);

    reply_VFS_data(request, buffer, size_eout + size_oout);

}

struct entry_s *_fs_common_create_entry(struct service_context_s *ctx, struct entry_s *parent, struct name_s *xname, struct system_stat_s *stat, unsigned int size, unsigned int flags, unsigned int *error)
{
    struct create_entry_s ce;
    unsigned int dummy=0;

    if (error==0) error=&dummy;
    init_create_entry(&ce, xname, parent, NULL, NULL, ctx, stat, NULL);
    return create_entry_extended(&ce);
}

struct entry_s *_fs_common_create_entry_unlocked(struct service_context_s *ctx, struct directory_s *directory, struct name_s *xname, struct system_stat_s *stat, unsigned int size, unsigned int flags, unsigned int *error)
{
    struct create_entry_s ce;
    unsigned int dummy=0;

    if (error==0) error=&dummy;
    init_create_entry(&ce, xname, NULL, directory, NULL, ctx, stat, NULL);
    enable_ce_extended_batch(&ce);
    return create_entry_extended(&ce);
}

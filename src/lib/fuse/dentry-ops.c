/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "dentry.h"
#include "log.h"
#include "datatypes.h"
#include "workspace.h"
#include "fuse.h"

static unsigned int _get_pathlen_dir(struct service_context_s *ctx, struct entry_s *entry)
{
    struct directory_s *d=get_directory(entry->inode);
    logoutput_debug("_get_pathlen_dir");
    return (d->getpath->get_pathlen)(ctx, d);
}

static void _append_path_dir(struct service_context_s *ctx, struct entry_s *entry, struct fuse_path_s *fp)
{
    struct directory_s *d=get_directory(entry->inode);
    logoutput_debug("_append_path_dir");
    get_service_context_path(ctx, d, fp);
}

static unsigned int _get_pathlen_nondir(struct service_context_s *ctx, struct entry_s *entry)
{
    struct directory_s *d=get_upper_directory_entry(entry);
    logoutput_debug("_get_pathlen_nondir");
    /* extra name plus slash */
    return 1 + entry->name.len + (d->getpath->get_pathlen)(ctx, d);
}

static void _append_path_nondir(struct service_context_s *ctx, struct entry_s *entry, struct fuse_path_s *fp)
{
    struct directory_s *d=get_upper_directory_entry(entry);

    logoutput_debug("_append_path_nondir");

    /* entry */
    fp->pathstart-=entry->name.len;
    memcpy(fp->pathstart, entry->name.name, entry->name.len);

    /* slash */
    fp->pathstart--;
    *fp->pathstart='/';

    get_service_context_path(ctx, d, fp);
}

static unsigned int _get_pathlen_zero(struct service_context_s *ctx, struct entry_s *entry)
{
    return 0;
}


static void _append_path_zero(struct service_context_s *ctx, struct entry_s *entry, struct fuse_path_s *fp)
{
    /* does nothing*/
}
static struct entry_ops_s entry_ops_zero = 
{
    .get_pathlen			= _get_pathlen_zero,
    .append_path			= _append_path_zero,
};

static struct entry_ops_s entry_ops_dir = {
    .get_pathlen			= _get_pathlen_dir,
    .append_path			= _append_path_dir,
};

static struct entry_ops_s entry_ops_nondir = {
    .get_pathlen			= _get_pathlen_nondir,
    .append_path			= _append_path_nondir,
};

void set_entry_ops(struct entry_s *entry)
{
    struct inode_s *inode=entry->inode;

    if (inode==NULL) {

	entry->ops=&entry_ops_zero;

    } else if (S_ISDIR(inode->st.st_mode)) {

	entry->ops=&entry_ops_dir;

    } else {

	entry->ops=&entry_ops_nondir;

    }

}

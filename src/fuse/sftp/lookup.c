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
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"
#include "path.h"
#include "handle.h"

/*
    common functions to do a
    LOOKUP
    of a name on sftp map
*/

// static unsigned int _sftp_cb_cache_size(struct create_entry_s *ce)
//{
//    struct attr_response_s *attr=(struct attr_response_s *) ce->cache.link.link.ptr;
//    return attr->size;
//}

void _sftp_lookup_entry_created(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *ctx=ce->context;
    struct context_interface_s *i=&ctx->interface;
    struct attr_context_s *attrctx=get_sftp_attr_context(i);
    struct sftp_reply_s *reply=(struct sftp_reply_s *) ce->cache.ptr;
    struct fuse_request_s *req=(struct fuse_request_s *) ce->ptr;
    struct inode_s *inode=entry->inode;
    struct system_stat_s *stat=&inode->stat;
    struct directory_s *directory=(* ce->get_directory)(ce);
    struct inode_s *pinode=directory->inode;
    struct attr_buffer_s abuff;

    set_sftp_stat_defaults(i, stat);
    set_attr_buffer_read(&abuff, reply->data, reply->size);
    read_sftp_attributes(attrctx, &abuff, stat);

    inode->nlookup=1;
    add_inode_context(ctx, inode);
    (* pinode->fs->type.dir.use_fs)(ctx, inode);
    get_current_time_system_time(&inode->stime);

    if (system_stat_test_ISDIR(&inode->stat)) {

	set_nlink_system_stat(stat, 2);
	increase_nlink_system_stat(&pinode->stat, 1);
	set_ctime_system_stat(&pinode->stat, &inode->stime); /* attribute of parent changed */
	assign_directory_inode(inode);

    } else {

	set_nlink_system_stat(stat, 1);

    }

    _fs_common_cached_lookup(ctx, req, inode); /* reply FUSE/VFS */
    set_mtime_system_stat(&pinode->stat, &inode->stime); /* entry added: parent directory is modified */
    adjust_pathmax(get_root_context(ctx), ce->pathlen);

}

void _sftp_lookup_entry_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *ctx=ce->context;
    struct context_interface_s *i=&ctx->interface;
    struct attr_context_s *attrctx=get_sftp_attr_context(i);
    struct fuse_request_s *req=(struct fuse_request_s *) ce->ptr;
    struct sftp_reply_s *reply=(struct sftp_reply_s *) ce->cache.ptr;
    struct inode_s *inode=entry->inode;
    struct system_stat_s *stat=&inode->stat;
    struct system_timespec_s mtime;
    struct attr_buffer_s abuff;
 
    get_mtime_system_stat(stat, &mtime);

    set_attr_buffer_read(&abuff, reply->data, reply->size);
    read_sftp_attributes(attrctx, &abuff, stat);
    if (test_remote_file_modified(stat, &mtime)==1) inode->flags |= INODE_FLAG_REMOTECHANGED;

    inode->nlookup++;
    _fs_common_cached_lookup(ctx, req, inode);
    get_current_time_system_time(&inode->stime);
    if (inode->nlookup==1) adjust_pathmax(get_root_context(ctx), ce->pathlen);

}

void _sftp_lookup_entry_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int errcode)
{
    struct fuse_request_s *request=(struct fuse_request_s *) ce->ptr;
    reply_VFS_error(request, errcode);
}

struct _cb_lookup_hlpr_s {
    struct fuse_request_s			*request;
    struct inode_s				*pinode;
    struct name_s 				*xname;
    unsigned int				pathlen;
};

static void _cb_success_lookup(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_lookup_hlpr_s *hlpr=(struct _cb_lookup_hlpr_s *) ptr;
    struct entry_s *entry=NULL;
    struct create_entry_s ce;

    init_create_entry(&ce, hlpr->xname, hlpr->pinode->alias, NULL, NULL, ctx, NULL, (void *) hlpr->request);
    ce.cache.ptr=(void *) reply;
    ce.pathlen=hlpr->pathlen;
    ce.cb_created=_sftp_lookup_entry_created;
    ce.cb_found=_sftp_lookup_entry_found;
    ce.cb_error=_sftp_lookup_entry_error;
    entry=create_entry_extended(&ce);
}

static void _cb_error_lookup(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_lookup_hlpr_s *hlpr=(struct _cb_lookup_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);

    if (errcode==ENOENT) {
	struct directory_s *directory=get_directory(ctx, hlpr->pinode, 0);
	unsigned int tmp=0;
	struct entry_s *entry=entry=find_entry(directory, hlpr->xname, &tmp);

	if (entry) queue_inode_2forget(get_root_context(ctx), get_ino_system_stat(&entry->inode->stat), 0, 0);

    }

}

static unsigned char _cb_interrupted_lookup(void *ptr)
{
    struct _cb_lookup_hlpr_s *hlpr=(struct _cb_lookup_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_lookup(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, struct name_s *xname, struct fuse_path_s *fpath)
{
    struct _cb_lookup_hlpr_s hlpr;
    unsigned int mask=(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_SIZE); /* basic stats */

    hlpr.request=request;
    hlpr.pinode=pinode;
    hlpr.xname=xname;
    hlpr.pathlen=(unsigned int)(fpath->path + fpath->len - fpath->pathstart); /* to determine the buffer is still big enough to hold every path on the workspace */

    _sftp_path_getattr(ctx, fpath, mask, 0, "getattr", _cb_success_lookup, _cb_error_lookup, _cb_interrupted_lookup, (void *) &hlpr);
}


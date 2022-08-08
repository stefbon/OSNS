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
#include "libosns-fuse.h"
#include "libosns-context.h"

#include "receive.h"
#include "request.h"
#include "fs-common.h"
#include "fs-service-path.h"

const char *dotdotname="..";
const char *dotname=".";
const struct name_s dotxname={".", 1, 0};
const struct name_s dotdotxname={"..", 2, 0};

extern struct fuse_config_s *get_fuse_interface_config(struct context_interface_s *i);

/* provides stat to lookup when entry already exists (is cached) */

void _fs_common_cached_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct fuse_config_s *config=get_fuse_interface_config(request->interface);
    struct fuse_entry_out out;
    struct system_timespec_s *attr_timeout=&config->attr_timeout;
    struct system_timespec_s *entry_timeout=&config->entry_timeout;

    logoutput_debug("_fs_common_cached_lookup: ino %li name %.*s", get_ino_system_stat(&inode->stat), inode->alias->name.len, inode->alias->name.name);
    log_inode_information(inode, INODE_INFORMATION_NAME | INODE_INFORMATION_NLOOKUP | INODE_INFORMATION_MODE | INODE_INFORMATION_SIZE | INODE_INFORMATION_MTIM | INODE_INFORMATION_INODE_LINK | INODE_INFORMATION_FS_COUNT);

    inode->nlookup++;

    memset(&out, 0, sizeof(struct fuse_entry_out));
    out.nodeid=get_ino_system_stat(&inode->stat);
    out.generation=0; /* todo: add a generation field to reuse existing inodes */
    out.entry_valid=get_system_time_sec(entry_timeout);
    out.entry_valid_nsec=get_system_time_nsec(entry_timeout);
    out.attr_valid=get_system_time_sec(attr_timeout);
    out.attr_valid_nsec=get_system_time_nsec(attr_timeout);
    fill_fuse_attr_system_stat(&out.attr, &inode->stat);

    reply_VFS_data(request, (char *) &out, sizeof(struct fuse_entry_out));

}

void _fs_common_virtual_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(context);
    struct entry_s *parent=pinode->alias, *entry=NULL;
    struct name_s xname={NULL, 0, 0};
    unsigned int error=0;
    struct directory_s *pdirectory=NULL;

    logoutput("_fs_common_virtual_lookup: name %.*s parent %li (thread %i)", len, name, (long) get_ino_system_stat(&pinode->stat), (int) gettid());

    xname.name=(char *)name;
    xname.len=strlen(name);
    calculate_nameindex(&xname);

    pdirectory=get_directory(w, pinode, GET_DIRECTORY_FLAG_NOCREATE);
    logoutput("_fs_common_virtual_lookup: find %.*s pdir %s", len, name, (pdirectory ? "DEF" : "UNDEF"));
    entry=find_entry(pdirectory, &xname, &error);

    if (entry) {
	struct inode_s *inode=entry->inode;
	struct directory_s *directory=NULL;
	struct data_link_s *link=NULL;
	struct service_context_s *tmp=NULL;

	logoutput("_fs_common_virtual_lookup: found ino  %lu %.*s", ((inode) ? get_ino_system_stat(&inode->stat) : 0), entry->name.len, entry->name.name);

	directory=get_directory(w, inode, GET_DIRECTORY_FLAG_NOCREATE);

	link=((directory) ? (&directory->link) : NULL);
	tmp=((link && (link->type==DATA_LINK_TYPE_CONTEXT)) ? (struct service_context_s *) ((char *) link - offsetof(struct service_context_s, link)) : NULL);

	/* it's possible that the entry represents the root of a service
	    in that case do a lookup of the '.' on the root of the service using the service specific fs calls
	*/

	log_inode_information(inode, INODE_INFORMATION_NAME | INODE_INFORMATION_NLOOKUP | INODE_INFORMATION_MODE | INODE_INFORMATION_SIZE | INODE_INFORMATION_MTIM | INODE_INFORMATION_INODE_LINK | INODE_INFORMATION_FS_COUNT);

	logoutput("_fs_common_virtual_lookup: found entry %.*s ino %li nlookup %i", entry->name.len, entry->name.name, get_ino_system_stat(&inode->stat), inode->nlookup);
	inode->nlookup++;

	if (tmp && (tmp->type==SERVICE_CTX_TYPE_FILESYSTEM)) {
	    unsigned int pathlen=get_pathmax(w) + 1;
	    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
	    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
	    struct path_service_fs_s *fs=tmp->service.filesystem.fs;

	    logoutput("_fs_common_virtual_lookup: use context %s", tmp->name);

	    init_fuse_path(fpath, pathlen + 1);
	    start_directory_fpath(fpath);

	    (* fs->lookup_existing)(tmp, request, entry, fpath);
	    return;

	}

	_fs_common_cached_lookup(context, request, inode);

    } else {

	logoutput("_fs_common_virtual_lookup: enoent lookup %i", (entry) ? entry->inode->nlookup : 0);
	reply_VFS_error(request, ENOENT);

    }

}

void _fs_common_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct fuse_config_s *config=get_fuse_interface_config(request->interface);
    struct fuse_attr_out out;
    struct system_timespec_s *attr_timeout=&config->attr_timeout;

    logoutput("_fs_common_getattr: context %s", context->name);

    memset(&out, 0, sizeof(struct fuse_attr_out));
    out.attr_valid=get_system_time_sec(attr_timeout);
    out.attr_valid_nsec=get_system_time_nsec(attr_timeout);
    fill_fuse_attr_system_stat(&out.attr, &inode->stat);

    reply_VFS_data(request, (char *) &out, sizeof(struct fuse_attr_out));

}

void _fs_common_access(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, unsigned int mask)
{

    if (mask==F_OK) {

	reply_VFS_error(request, 0);
	return;

    } else {
	uid_t uid=getuid();
	gid_t gid=getgid();

	if (request->uid==uid) {

	    reply_VFS_error(request, 0);
	    return;

	}

    }

    reply_VFS_error(request, EACCES);

}

static struct entry_s *get_first_entry_directory(struct directory_s *d)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) d->buffer;
    struct list_element_s *list=get_list_head(&sl->header, 0);
    return ((list) ? ((struct entry_s *)((char *) list - offsetof(struct entry_s, list))) : NULL);
}

void _fs_common_virtual_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    struct fuse_config_s *config=NULL;
    struct workspace_mount_s *w=NULL;
    struct directory_s *directory=NULL;
    struct fuse_open_out out;
    unsigned int tmp=0;

    logoutput("_fs_common_virtual_opendir");

    config=get_fuse_interface_config(request->interface);
    w=get_workspace_mount_ctx(opendir->context);

    directory=get_directory(w, opendir->inode, GET_DIRECTORY_FLAG_NOCREATE);
    if (directory && get_directory_count(directory)>0) flags |= FUSE_OPENDIR_FLAG_NONEMPTY;

    memset(&out, 0, sizeof(struct fuse_open_out));
    out.fh=(uint64_t) opendir;
    out.open_flags=0;
    reply_VFS_data(request, (char *) &out, sizeof(struct fuse_open_out));

    opendir->readdir=_fs_common_virtual_readdir;
    opendir->releasedir=_fs_common_virtual_releasedir;

    tmp=(FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS | FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS) ; /* sane flags */
    if (config->flags & FUSE_CONFIG_FLAG_HIDE_SYSTEMFILES) tmp |= FUSE_OPENDIR_FLAG_HIDE_DOTFILES;
    signal_set_flag(opendir->signal, &opendir->flags, tmp);

}

void _fs_common_virtual_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset)
{
    struct fuse_config_s *config=get_fuse_interface_config(request->interface);
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct directory_s *directory=NULL;
    struct name_s *xname=NULL;
    struct inode_s *inode=NULL;
    struct entry_s *entry=NULL;
    struct list_element_s *list=NULL;
    char buff[size];
    struct direntry_buffer_s direntries;
    unsigned int error=0;
    struct osns_lock_s rlock;
    uint64_t ino2keep=0;

    logoutput("_fs_common_virtual_readdir: size %i off %i flags %i", (unsigned int) size, (unsigned int) offset, opendir->flags);
    directory=get_directory(workspace, opendir->inode, 0);

    if (rlock_directory(directory, &rlock)==-1) {

	reply_VFS_error(request, EAGAIN);
	return;

    }

    direntries.data=buff;
    direntries.pos=0;
    direntries.size=size;
    direntries.offset=offset;

    if (opendir->flags & FUSE_OPENDIR_FLAG_READDIRPLUS) {

	opendir->add_direntry=add_direntry_plus_buffer;

    } else {

	opendir->add_direntry=add_direntry_buffer;

    }

    while ((direntries.pos < direntries.size) && ((request->flags & FUSE_REQUEST_FLAG_INTERRUPTED)==0) && (opendir->flags & (FUSE_OPENDIR_FLAG_FINISH | FUSE_OPENDIR_FLAG_INCOMPLETE | FUSE_OPENDIR_FLAG_ERROR))==0) {

	xname=NULL;

	if (direntries.offset==0) {

	    opendir->count_keep=get_directory_count(directory);
	    inode=opendir->inode;

    	    /* the . entry */

	    xname=(struct name_s *) &dotxname;
	    ino2keep=0;
	    entry=get_first_entry_directory(directory);

    	} else if (direntries.offset==1) {
    	    struct entry_s *parent=NULL;

	    /* the .. entry */

	    xname=(struct name_s *) &dotdotxname;
	    ino2keep=0;

	    inode=opendir->inode;
	    parent=get_parent_entry(inode->alias);
	    if (parent) inode=parent->inode;

    	} else {

	    if (opendir->ino>0) {

		logoutput("_fs_common_virtual_readdir: lookup ino %li", opendir->ino);

		inode=lookup_workspace_inode(workspace, opendir->ino);
		opendir->ino=0;
		if (inode) entry=inode->alias;

	    }

	    readdir:

	    if (entry==NULL) {

		opendir->flags |= FUSE_OPENDIR_FLAG_FINISH;
		break;

	    }

	    inode=entry->inode;
	    memcpy(&inode->stime, &directory->synctime, sizeof (struct timespec));
	    if ((* opendir->hidefile)(opendir, entry)) {

		entry=get_next_entry(entry);
		if (entry) ino2keep=get_ino_system_stat(&entry->inode->stat);
		continue;

	    }

	    xname=&entry->name;
	    entry=get_next_entry(entry);
	    if (entry) ino2keep=get_ino_system_stat(&entry->inode->stat);

	}

	/* add entry to the buffer to send to VFS: does it fit, if no keep the ino for adding later next batch */

	if ((* opendir->add_direntry)(config, &direntries, xname, &inode->stat)==-1) {

	    logoutput("_fs_common_virtual_readdir: %.*s does not fit in buffer", xname->len, xname->name);
	    opendir->ino=ino2keep; /* keep it for the next batch */
	    break;

	}

	logoutput("_fs_common_virtual_readdir: add %.*s", xname->len, xname->name);

    }

    unlock_directory(directory, &rlock);
    reply_VFS_data(request, direntries.data, direntries.pos);
}

void _fs_common_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset)
{
    struct fuse_config_s *config=get_fuse_interface_config(request->interface);
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct directory_s *directory=NULL;
    struct name_s *xname=NULL;
    struct inode_s *inode=NULL;
    struct entry_s *entry=NULL;
    char buff[size];
    struct direntry_buffer_s direntries;
    unsigned int error=0;
    struct osns_lock_s rlock;
    uint64_t ino2keep=0;

    logoutput("_fs_common_readdir: size %i off %i flags %i", (unsigned int) size, (unsigned int) offset, opendir->flags);
    directory=get_directory(workspace, opendir->inode, 0);

    if (rlock_directory(directory, &rlock)==-1) {

	reply_VFS_error(request, EAGAIN);
	return;

    }

    direntries.data=buff;
    direntries.pos=0;
    direntries.size=size;
    direntries.offset=offset;

    if (opendir->flags & FUSE_OPENDIR_FLAG_READDIRPLUS) {

	opendir->add_direntry=add_direntry_plus_buffer;

    } else {

	opendir->add_direntry=add_direntry_buffer;

    }

    while ((direntries.pos < direntries.size) && (request->flags & FUSE_REQUEST_FLAG_INTERRUPTED)==0 && (opendir->flags & (FUSE_OPENDIR_FLAG_FINISH | FUSE_OPENDIR_FLAG_INCOMPLETE | FUSE_OPENDIR_FLAG_ERROR))==0) {

	entry=NULL;
	xname=NULL;

	if (direntries.offset==0) {

	    opendir->count_keep=get_directory_count(directory);
	    inode=opendir->inode;

    	    /* the . entry */

	    xname=(struct name_s *) &dotxname;
	    ino2keep=0;

    	} else if (direntries.offset==1) {
    	    struct entry_s *parent=NULL;

	    /* the .. entry */

	    xname=(struct name_s *) &dotdotxname;
	    ino2keep=0;

	    inode=opendir->inode;
	    entry=inode->alias;
	    parent=get_parent_entry(entry);
	    if (parent) inode=parent->inode;

    	} else {

	    if (opendir->ino>0) {

		inode=lookup_workspace_inode(workspace, opendir->ino);
		opendir->ino=0;
		if (inode) entry=inode->alias;

	    }

	    readdir:

	    if (entry==NULL) {

		if ((entry=get_fuse_direntry(opendir, request))==NULL) {

		    set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_FINISH);
		    break;

		}

		inode=entry->inode;
		memcpy(&inode->stime, &directory->synctime, sizeof (struct timespec));

	    }

	    xname=&entry->name;
	    ino2keep=inode->stat.sst_ino;

	}

	/* add entry to the buffer to send to VFS: does it fit, if no keep the ino for adding later next batch */

	if ((* opendir->add_direntry)(config, &direntries, xname, &inode->stat)==-1) {

	    logoutput("_fs_common_readdir: %.*s does not fit in buffer", xname->len, xname->name);
	    opendir->ino=ino2keep; /* keep it for the next batch */
	    break;

	}

	logoutput("_fs_common_readdir: add %.*s", xname->len, xname->name);

    }

    unlock_directory(directory, &rlock);
    reply_VFS_data(request, direntries.data, direntries.pos);
    // if (opendir->flags & FUSE_OPENDIR_FLAG_QUEUE_READY) opendir->flags |= FUSE_OPENDIR_FLAG_FINISH;

}

void _fs_common_virtual_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *request)
{
    signal_set_flag(opendir->signal, &opendir->flags, FUSE_OPENDIR_FLAG_FINISH);
    reply_VFS_error(request, 0);
}

void _fs_common_virtual_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync)
{
    reply_VFS_error(request, 0);
}

void _fs_common_remove_nonsynced_dentries(struct fuse_opendir_s *opendir)
{
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct directory_s *directory=get_directory(workspace, opendir->inode, 0);
    struct osns_lock_s wlock;

    if (wlock_directory(directory, &wlock)==0) {

	if ((opendir->count_keep > opendir->count_found) || (opendir->count_created + opendir->count_found != get_directory_count(directory))) {
	    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
	    struct inode_s *inode=NULL;
	    struct entry_s *entry=NULL;
	    struct list_element_s *next=NULL;
	    struct list_element_s *list=get_list_head(&sl->header, 0);

	    while (list) {

		next=get_next_element(list);
		entry=(struct entry_s *)((char *) list - offsetof(struct entry_s, list));

		if (entry->flags & _ENTRY_FLAG_SPECIAL) goto next;
		inode=entry->inode;
		if (system_time_test_earlier(&inode->stime, &directory->synctime) == 1) queue_inode_2forget(workspace, inode->stat.sst_ino, FORGET_INODE_FLAG_DELETED, 0);

		next:
		list=next;

	    }

	}

	unlock_directory(directory, &wlock);

    }

}

void _fs_common_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct fuse_statfs_out out;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

    memset(&out, 0, sizeof(struct fuse_statfs_out));

    out.st.blocks=0;
    out.st.bfree=UINT32_T_MAX;
    out.st.bavail=UINT32_T_MAX;
    out.st.bsize=4096;

    out.st.files=(uint64_t) workspace->inodes.nrinodes;
    out.st.ffree=(uint64_t) (UINT32_T_MAX - out.st.files);

    out.st.namelen=255; /* a sane default */
    out.st.frsize=out.st.bsize; /* use the same as block size */

    reply_VFS_data(request, (char *) &out, sizeof(struct fuse_statfs_out));

}

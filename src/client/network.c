/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "fuse/browse-fs.h"

#include "osns_client.h"
#include "network.h"

struct entry_s *create_network_map_entry(struct service_context_s *context, struct directory_s *directory, struct name_s *xname, unsigned int *error)
{
    struct create_entry_s ce;
    struct system_stat_s stat;
    struct system_timespec_s tmp=SYSTEM_TIME_INIT;

    /* stat values for a network map */

    memset(&stat, 0, sizeof(struct system_stat_s));

    set_type_system_stat(&stat, S_IFDIR);
    set_mode_system_stat(&stat, S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    set_uid_system_stat(&stat, 0);
    set_gid_system_stat(&stat, 0);
    set_ino_system_stat(&stat, 0);
    set_nlink_system_stat(&stat, 2);
    set_size_system_stat(&stat, _INODE_DIRECTORY_SIZE);
    set_blksize_system_stat(&stat, 4096);
    calc_blocks_system_stat(&stat);

    get_current_time_system_time(&tmp);
    set_atime_system_stat(&stat, &tmp);
    set_mtime_system_stat(&stat, &tmp);
    set_ctime_system_stat(&stat, &tmp);
    set_btime_system_stat(&stat, &tmp);

    init_create_entry(&ce, xname, NULL, directory, NULL, context, &stat, NULL);

    enable_ce_extended_batch(&ce);
    disable_ce_extended_adjust_pathmax(&ce);

    return create_entry_extended(&ce);

}

struct entry_s *install_virtualnetwork_map(struct service_context_s *context, struct entry_s *parent, char *name, const char *what, unsigned char *p_action)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct entry_s *entry=NULL;
    unsigned int error=0;
    struct directory_s *pdirectory=get_directory(workspace, parent->inode, 0);
    struct osns_lock_s wlock;

    if (wlock_directory(pdirectory, &wlock)==0) {
	struct name_s xname;

	xname.name=name;
	xname.len=strlen(name);
	calculate_nameindex(&xname);

	entry=find_entry_batch(pdirectory, &xname, &error);

	/* only install if not exists */

	if (entry) {

	    logoutput_debug("install_virtualnetwork_map: map %s already exists", name);
	    if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_FOUND;
	    goto unlock;

	}

	error=0;
	entry=create_network_map_entry(context, pdirectory, &xname, &error);

	if (entry==NULL) {

	    logoutput_warning("install_virtualnetwork_map: unable to create map %s", name);
	    if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_ERROR;
	    goto unlock;

	}

	logoutput_debug("install_virtualnetwork_map: created map %s for %s", name, ((what) ? what : "unknown"));
	if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_ADDED;

	if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	    use_service_browse_fs(context, entry->inode);

	} else if (context->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    use_service_path_fs(context, entry->inode);

	} else {

	    inherit_fuse_fs_parent(context, entry->inode);

	}

	unlock:
	unlock_directory(pdirectory, &wlock);

    } else {

	if (p_action) *p_action=FUSE_NETWORK_ACTION_FLAG_ERROR;

    }

    out:
    return entry;

}


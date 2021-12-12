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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "interface/sftp-attr.h"

#include "inode-stat.h"

/* TODO: use in stead of setting this and copy it to the inode->stat, use the inode->stat from the first moment, and use some custom "set" functions
    same as the mapping of users: use a parse_time_net2local and vice versa */

void set_local_attributes(struct context_interface_s *interface, struct inode_s *inode, struct system_stat_s *stat)
{

    if (stat->mask & SYSTEM_STAT_TYPE) set_type_system_stat(&inode->stat, get_type_system_stat(stat));
    if (stat->mask & SYSTEM_STAT_MODE) set_mode_system_stat(&inode->stat, get_mode_system_stat(stat));
    if (stat->mask & SYSTEM_STAT_SIZE) set_size_system_stat(&inode->stat, get_size_system_stat(stat));
    if (stat->mask & SYSTEM_STAT_UID) set_uid_system_stat(&inode->stat, get_uid_system_stat(stat));
    if (stat->mask & SYSTEM_STAT_GID) set_gid_system_stat(&inode->stat, get_gid_system_stat(stat));

    if (stat->mask & SYSTEM_STAT_ATIME) {

	// correct_time_s2c_ctx(interface, &stat->atime);

	copy_atime_system_stat(&inode->stat, stat);

    }

    if (stat->mask & SYSTEM_STAT_MTIME) {


	// correct_time_s2c_ctx(interface, &stat->mtime);
	copy_mtime_system_stat(&inode->stat, stat);

    }

    if (stat->mask & SYSTEM_STAT_CTIME) {

	// correct_time_s2c_ctx(interface, &stat->ctime);
	copy_ctime_system_stat(&inode->stat, stat);

    }

    if (stat->mask & SYSTEM_STAT_BTIME) {

	// correct_time_s2c_ctx(interface, &stat->atime);
	copy_btime_system_stat(&inode->stat, stat);

    }

}

static void _get_supported_sftp_attr_cb(unsigned int stat_mask, unsigned int len, unsigned int valid, unsigned int fattr, void *ptr)
{
    struct get_supported_sftp_attr_s *gssa=(struct get_supported_sftp_attr_s *) ptr;

    if (stat_mask & gssa->stat_mask_asked) {

	gssa->stat_mask_result |= stat_mask;
	gssa->len += len;
	gssa->valid.mask |= valid;

    }

}

unsigned int get_attr_buffer_size(struct context_interface_s *interface, struct rw_attr_result_s *r, struct system_stat_s *stat, struct get_supported_sftp_attr_s *gssa)
{

    gssa->stat_mask_asked=stat->mask;
    gssa->stat_mask_result=0;
    gssa->len=0;
    init_sftp_valid(&gssa->valid);

    parse_attributes_generic_ctx(interface, r, NULL, 'w', _get_supported_sftp_attr_cb, (void *) gssa);
    return gssa->len;

}

void set_sftp_inode_stat_defaults(struct context_interface_s *interface, struct inode_s *inode)
{
    struct system_stat_s *stat=&inode->stat;
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    stat->sst_uid=get_sftp_unknown_userid_ctx(interface);
    stat->sst_gid=get_sftp_unknown_groupid_ctx(interface);

    /* set time to this moment */

    get_current_time_system_time(&time);
    set_atime_system_stat(stat, &time);
    set_btime_system_stat(stat, &time);
    set_ctime_system_stat(stat, &time);
    set_mtime_system_stat(stat, &time);

}

int compare_cache_sftp(struct ssh_string_s *data, unsigned int size, char *buffer, void *ptr)
{
    return (size>=data->len && memcmp(buffer, data->ptr, data->len)==0) ? 0 : 1;
}

int test_remote_file_changed(struct system_stat_s *stat, struct system_timespec_s *mtime_before)
{
    struct system_timespec_s mtime=SYSTEM_TIME_INIT;

    get_mtime_system_stat(stat, &mtime);
    return compare_system_times(mtime_before, &mtime);
}

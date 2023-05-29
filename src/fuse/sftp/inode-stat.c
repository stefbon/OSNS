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
#include "sftp/rw-attr-generic.h"
#include "interface/sftp-attr.h"

#include "inode-stat.h"

/* TODO: use in stead of setting this and copy it to the inode->stat, use the inode->stat from the first moment, and use some custom "set" functions
    same as the mapping of users: use a parse_time_net2local and vice versa */

void set_local_attributes(struct context_interface_s *i, struct system_stat_s *stat, struct system_stat_s *stat2set)
{

    if (stat2set->mask & SYSTEM_STAT_TYPE) set_type_system_stat(stat, get_type_system_stat(stat2set));
    if (stat2set->mask & SYSTEM_STAT_MODE) set_mode_system_stat(stat, get_mode_system_stat(stat2set));
    if (stat2set->mask & SYSTEM_STAT_SIZE) set_size_system_stat(stat, get_size_system_stat(stat2set));
    if (stat2set->mask & SYSTEM_STAT_UID) set_uid_system_stat(stat, get_uid_system_stat(stat2set));
    if (stat2set->mask & SYSTEM_STAT_GID) set_gid_system_stat(stat, get_gid_system_stat(stat2set));

    if (stat2set->mask & SYSTEM_STAT_ATIME) copy_atime_system_stat(stat, stat2set);
    if (stat2set->mask & SYSTEM_STAT_MTIME) copy_mtime_system_stat(stat, stat2set);
    if (stat2set->mask & SYSTEM_STAT_CTIME) copy_ctime_system_stat(stat, stat2set);
    if (stat2set->mask & SYSTEM_STAT_BTIME) copy_btime_system_stat(stat, stat2set);

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

unsigned int get_attr_buffer_size(struct context_interface_s *i, struct rw_attr_result_s *r, struct system_stat_s *stat, struct get_supported_sftp_attr_s *gssa)
{

    gssa->stat_mask_asked=stat->mask;
    gssa->stat_mask_result=0;
    gssa->len=0;
    init_sftp_valid(&gssa->valid);

    parse_attributes_generic_ctx(i, r, NULL, 'w', _get_supported_sftp_attr_cb, (void *) gssa);
    return gssa->len;

}

int compare_cache_sftp(struct ssh_string_s *data, unsigned int size, char *buffer, void *ptr)
{
    return (size>=data->len && memcmp(buffer, data->ptr, data->len)==0) ? 0 : 1;
}

int test_remote_file_modified(struct system_stat_s *stat, struct system_timespec_s *mtime_before)
{
    struct system_timespec_s mtime=SYSTEM_TIME_INIT;

    get_mtime_system_stat(stat, &mtime);
    return system_time_test_earlier(mtime_before, &mtime);
}

void set_sftp_stat_defaults(struct context_interface_s *i, struct system_stat_s *stat)
{
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    stat->sst_uid=get_sftp_unknown_userid_ctx(i);
    stat->sst_gid=get_sftp_unknown_groupid_ctx(i);

    /* set time to this moment */

    get_current_time_system_time(&time);
    set_atime_system_stat(stat, &time);
    set_btime_system_stat(stat, &time);
    set_ctime_system_stat(stat, &time);
    set_mtime_system_stat(stat, &time);

}

void read_sftp_attributes(struct attr_context_s *attrctx, struct attr_buffer_s *abuff, struct system_stat_s *stat)
{
    unsigned int valid=(* abuff->ops->rw.read.read_uint32)(abuff);
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    read_attributes_generic(attrctx, abuff, &r, stat, valid);
}

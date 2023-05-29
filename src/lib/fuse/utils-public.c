/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Stef Bon <stefbon@gmail.com>

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

#include <dirent.h>
#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-eventloop.h"

#include "lib/system/stat.h"

#include "defaults.h"
#include "receive.h"
#include "utils.h"
#include "config.h"
#include "dentry.h"
#include "request.h"

#include "utils-public.h"

static void copy_stat_direntry_buffer(struct fuse_dirent *dirent, struct name_s *xname, struct system_stat_s *stat, off_t offset)
{
    unsigned int type=get_type_system_stat(stat);

    dirent->ino=get_ino_system_stat(stat);
    dirent->off=(offset + 1);
    dirent->namelen=xname->len;
    dirent->type=((type) ? (type >> 12) : DT_UNKNOWN);
    memcpy(dirent->name, xname->name, xname->len);
}

int add_direntry_buffer(struct fuse_config_s *c, struct direntry_buffer_s *buffer, struct name_s *xname, struct system_stat_s *stat)
{
    size_t dirent_size=offsetof(struct fuse_dirent, name) + xname->len;
    size_t dirent_size_alligned=(((dirent_size) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));

    if (dirent_size_alligned <= (buffer->size - buffer->pos)) {
	struct fuse_dirent *dirent=(struct fuse_dirent *) &buffer->data[buffer->pos];

	copy_stat_direntry_buffer(dirent, xname, stat, buffer->offset);

	buffer->pos += dirent_size_alligned;
	buffer->offset++;
	return 0;

    }

    return -1;

}

int add_direntry_plus_buffer(struct fuse_config_s *c, struct direntry_buffer_s *buffer, struct name_s *xname, struct system_stat_s *stat)
{
    size_t dirent_size=offsetof(struct fuse_direntplus, dirent.name) + xname->len;
    size_t dirent_size_alligned=(((dirent_size) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));

    if (dirent_size_alligned <= (buffer->size - buffer->pos)) {
	struct fuse_direntplus *direntplus=(struct fuse_direntplus *) &buffer->data[buffer->pos];
	struct fuse_dirent *dirent=&direntplus->dirent;

	direntplus->entry_out.nodeid=get_ino_system_stat(stat);
	direntplus->entry_out.generation=0;
	direntplus->entry_out.entry_valid=get_system_time_sec(&c->entry_timeout);
	direntplus->entry_out.entry_valid_nsec=get_system_time_nsec(&c->entry_timeout);
	direntplus->entry_out.attr_valid=get_system_time_sec(&c->attr_timeout);
	direntplus->entry_out.attr_valid_nsec=get_system_time_nsec(&c->attr_timeout);
	fill_fuse_attr_system_stat(&direntplus->entry_out.attr, stat);

	copy_stat_direntry_buffer(dirent, xname, stat, buffer->offset);

	buffer->pos += dirent_size_alligned;
	buffer->offset++;
	return 0;

    }

    return -1;

}

void set_default_fuse_timeout(struct system_timespec_s *timeout, unsigned char what)
{
    set_system_time(timeout, 1, 0);
}

void set_rootstat(struct system_stat_s *stat)
{
    struct system_timespec_s tmp=SYSTEM_TIME_INIT;

    memset(stat, 0, sizeof(struct system_stat_s));

    get_current_time_system_time(&tmp);

    set_ctime_system_stat(stat, &tmp);
    set_atime_system_stat(stat, &tmp);
    set_btime_system_stat(stat, &tmp);
    set_mtime_system_stat(stat, &tmp);

    set_ino_system_stat(stat, FUSE_ROOT_ID);

    set_type_system_stat(stat, S_IFDIR);
    set_mode_system_stat(stat, (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));

    set_nlink_system_stat(stat, 2);

    set_uid_system_stat(stat, 0);
    set_gid_system_stat(stat, 0);
    set_size_system_stat(stat, _INODE_DIRECTORY_SIZE);
    set_blksize_system_stat(stat, 4096);

    calc_blocks_system_stat(stat);

}

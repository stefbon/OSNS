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

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-sl.h"
#include "libosns-eventloop.h"
#include "libosns-system.h"

#include "utils.h"
#include "defaults.h"

void fill_fuse_attr_system_stat(struct fuse_attr *attr, struct system_stat_s *stat)
{

    attr->ino=get_ino_system_stat(stat);
    attr->size=get_size_system_stat(stat);
    attr->blksize=_DEFAULT_BLOCKSIZE;
    attr->blocks=calc_amount_blocks(attr->size, attr->blksize);

    attr->atime=(uint64_t) get_atime_sec_system_stat(stat);
    attr->atimensec=(uint64_t) get_atime_nsec_system_stat(stat);
    attr->mtime=(uint64_t) get_mtime_sec_system_stat(stat);
    attr->mtimensec=(uint64_t) get_mtime_nsec_system_stat(stat);
    attr->ctime=(uint64_t) get_ctime_sec_system_stat(stat);
    attr->ctimensec=(uint64_t) get_ctime_nsec_system_stat(stat);

    attr->mode=(get_type_system_stat(stat) | get_mode_system_stat(stat));
    attr->nlink=get_nlink_system_stat(stat);
    attr->uid=get_uid_system_stat(stat);
    attr->gid=get_gid_system_stat(stat);
    attr->rdev=0;

}


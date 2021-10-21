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
#include "system.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"

#include "sftp/attr-context.h"

#include "rw-attr-generic.h"
#include "write-attr-v03.h"

void write_attr_zero(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{}

void write_attr_size_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint64_t size=get_size_system_stat(stat);
    (* buffer->ops->rw.write.write_uint64)(buffer, size);
}

void write_attr_uidgid_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct net_entity_s user;
    struct net_entity_s group;

    user.local.uid=get_uid_system_stat(stat);
    (* ctx->mapping->mapcb.get_user_l2p)(ctx->mapping, &user);
    (* buffer->ops->rw.write.write_uint32)(buffer, user.net.id);

    group.local.gid=get_gid_system_stat(stat);
    (* ctx->mapping->mapcb.get_group_l2p)(ctx->mapping, &group);
    (* buffer->ops->rw.write.write_uint32)(buffer, group.net.id);

}

void write_attr_permissions_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t perm=get_mode_system_stat(stat) & (S_IRWXU | S_IRWXG | S_IRWXO); /* only interested in permission bits */
    uint32_t type=get_type_system_stat(stat);

    /* not documented: the permissions field in v03 containt the mode AND the type
	where later it only holds the permissions */

    (* buffer->ops->rw.write.write_uint32)(buffer, (uint32_t) (perm | type));

}

void write_attr_acmodtime_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    get_atime_system_stat(stat, &time);
    (* buffer->ops->rw.write.write_uint32)(buffer, time.sec);

    get_mtime_system_stat(stat, &time);
    (* buffer->ops->rw.write.write_uint32)(buffer, time.sec);

}

void write_name_name_response_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name)
{
    (* buffer->ops->rw.write.write_string)(buffer, name);
}

void write_attr_name_response_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{

    /* longname: an empty string, not important */

    (* buffer->ops->rw.write.write_uint32)(buffer, 0);

    /* attr */

    (* buffer->ops->rw.write.write_uint32)(buffer, r->valid);
    write_attributes_generic(actx, buffer, r, stat, r->valid);

}

/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#include "libosns-log.h"
#include "libosns-misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v04.h"

#include "sftp/attr-context.h"
#include "write-attr-v03.h"
#include "write-attr-v04.h"

static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, 0, 0, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, 0, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, 0};

void write_attr_type_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned int type=IFTODT(get_type_system_stat(stat));

    type=((type<13) ? type_reverse[type] : SSH_FILEXFER_TYPE_UNKNOWN);
    (* buffer->ops->rw.write.write_uchar)(buffer, type);
    logoutput_debug("write_attr_type_v04: type %i", type);
}

void write_attr_ownergroup_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct net_entity_s user;
    struct net_entity_s group;

    user.net.name.ptr=(char *) (buffer->buffer + buffer->pos + 4); /* use buffer to write name to */
    user.net.name.len=(unsigned int) (buffer->left - 4); /* available space */
    user.localid=get_uid_system_stat(stat);
    logoutput_debug("write_attr_ownergroup_v04: local uid %i", user.localid);
    (* actx->mapping->mapcb.get_user_l2p)(actx->mapping, &user);
    (* buffer->ops->rw.write.write_uint32)(buffer, user.net.name.len);
    (* buffer->ops->rw.write.write_skip)(buffer, user.net.name.len); /* name is already written above to the buffer */

    group.net.name.ptr=(char *) (buffer->buffer + buffer->pos + 4); /* use buffer to write name to */
    group.net.name.len=(unsigned int) (buffer->left - 4); /* available space */
    group.localid=get_gid_system_stat(stat);
    logoutput_debug("write_attr_ownergroup_v04: local gid %i", group.localid);
    (* actx->mapping->mapcb.get_group_l2p)(actx->mapping, &group);
    (* buffer->ops->rw.write.write_uint32)(buffer, group.net.name.len);
    (* buffer->ops->rw.write.write_skip)(buffer, group.net.name.len); /* name is already written above to the buffer */

    logoutput_debug("write_attr_ownergroup_v04: user %.*s group %.*s", user.net.name.len, user.net.name.ptr, group.net.name.len, group.net.name.ptr);

}

void write_attr_permissions_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t perm=get_mode_system_stat(stat) & (S_IRWXU | S_IRWXG | S_IRWXO); /* only interested in permission bits */

    (* buffer->ops->rw.write.write_uint32)(buffer, perm);
    logoutput_debug("write_attr_permissions_v04: perm %i", perm);
}

void write_attr_accesstime_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    int64_t sec=get_atime_sec_system_stat(stat);

    (* buffer->ops->rw.write.write_int64)(buffer, sec);
    logoutput_debug("write_attr_accesstime_v04: sec %li", sec);
}

void write_attr_accesstime_n_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t nsec=get_atime_nsec_system_stat(stat);
    (* buffer->ops->rw.write.write_uint32)(buffer, nsec);
    logoutput_debug("write_attr_accesstime_n_v04: nsec %i", nsec);
}

void write_attr_modifytime_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    int64_t sec=get_mtime_sec_system_stat(stat);

    (* buffer->ops->rw.write.write_int64)(buffer, sec);
    logoutput_debug("write_attr_modifytime_v04: sec %li", sec);
}

void write_attr_modifytime_n_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t nsec=get_mtime_nsec_system_stat(stat);
    (* buffer->ops->rw.write.write_uint32)(buffer, nsec);
    logoutput_debug("write_attr_modifytime_n_v04: nsec %i", nsec);
}

void write_name_name_response_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name)
{
    /* TODO: convert from local to UTF-8 (if required) */

    (* buffer->ops->rw.write.write_string)(buffer, name);

    logoutput_debug("write_name_name_response_v04: name %.*s", name->len, name->ptr);
}

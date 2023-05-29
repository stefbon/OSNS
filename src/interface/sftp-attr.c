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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/rw-attr-generic.h"
#include "sftp/attr.h"
#include "sftp/init.h"
#include "sftp-attr.h"

struct attr_context_s *get_sftp_attr_context(struct context_interface_s *i)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return &sftp->attrctx;
}

void parse_attributes_generic_ctx(struct context_interface_s *i, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char what, void (* cb)(unsigned int stat_mask, unsigned int len, unsigned int valid, unsigned int fattr, void *ptr), void *ptr)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    parse_sftp_attributes_stat_mask(&sftp->attrctx, r, stat, what, cb, ptr);
}

void read_sftp_attributes_ctx(struct context_interface_s *i, struct attr_buffer_s *abuff, struct system_stat_s *stat)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    unsigned int valid_bits=(* abuff->ops->rw.read.read_uint32)(abuff);
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    read_attributes_generic(&sftp->attrctx, abuff, &r, stat, valid_bits);
}

void write_attributes_ctx(struct context_interface_s *i, struct attr_buffer_s *abuff, struct rw_attr_result_s *r, struct system_stat_s *stat, struct sftp_valid_s *valid)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    write_attributes_generic(&sftp->attrctx, abuff, r, stat, valid);
}

void read_name_name_response_ctx(struct context_interface_s *i, struct attr_buffer_s *abuff, struct ssh_string_s *name)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    (* sftp->attrctx.ops.read_name_name_response)(&sftp->attrctx, abuff, name);
}

void correct_time_s2c_ctx(struct context_interface_s *i, struct system_timespec_s *t)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    (* sftp->time_ops.correct_time_s2c)(sftp, t);
}

void correct_time_c2s_ctx(struct context_interface_s *i, struct system_timespec_s *t)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    (* sftp->time_ops.correct_time_c2s)(sftp, t);
}

void enable_attributes_ctx(struct context_interface_s *i, struct sftp_valid_s *valid, const char *name)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    unsigned char enabled=(* sftp->attrctx.ops.enable_attr)(&sftp->attrctx, valid, name);
}

uid_t get_sftp_unknown_userid_ctx(struct context_interface_s *i)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return sftp->mapping->unknown_uid;
}

gid_t get_sftp_unknown_groupid_ctx(struct context_interface_s *i)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return sftp->mapping->unknown_gid;
}

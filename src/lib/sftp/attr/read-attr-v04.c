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

#include "libosns-log.h"
#include "libosns-misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v04.h"
#include "sftp/attr-context.h"

#include "read-attr-v03.h"
#include "read-attr-v04.h"

#include "datatypes/ssh-uint.h"

static unsigned int type_mapping[6]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0};

void read_attr_type_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned char tmp=(* buffer->ops->rw.read.read_uchar)(buffer);
    unsigned int type=(tmp<5) ? type_mapping[tmp] : 0;
    set_type_system_stat(stat, type);
    logoutput_debug("read_attr_type_v04: type %i", type);
}

struct _attr_cb_s {
    struct attr_context_s 		*ctx;
    struct rw_attr_result_s 		*r;
    struct system_stat_s 		*stat;
};

static void _attr_cb_user(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;
    struct attr_context_s *ctx=data->ctx;
    struct system_stat_s *stat=data->stat;
    struct net_entity_s user;

    user.net.name.ptr=s->ptr;
    user.net.name.len=s->len;

    (* ctx->mapping->mapcb.get_user_p2l)(ctx->mapping, &user);
    set_uid_system_stat(stat, user.localid);
}

static void _attr_cb_group(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;
    struct attr_context_s *ctx=data->ctx;
    struct system_stat_s *stat=data->stat;
    struct net_entity_s group;

    group.net.name.ptr=s->ptr;
    group.net.name.len=s->len;

    (* ctx->mapping->mapcb.get_group_p2l)(ctx->mapping, &group);
    set_gid_system_stat(stat, group.localid);
}

void read_attr_ownergroup_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct _attr_cb_s _cb_data={.ctx=ctx, .r=r, .stat=stat};
    uint32_t len=0;
    struct ssh_string_s username=SSH_STRING_INIT;
    struct ssh_string_s groupname=SSH_STRING_INIT;

    len=(* buffer->ops->rw.read.read_string)(buffer, &username, _attr_cb_user, (void *) &_cb_data);
    len=(* buffer->ops->rw.read.read_string)(buffer, &groupname, _attr_cb_group, (void *) &_cb_data);

    logoutput_debug("read_attr_ownergroup_v04: u %.*s g %.*s", username.len, username.ptr, groupname.len, groupname.ptr);

}

void read_attr_permissions_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned int mode=(* buffer->ops->rw.read.read_uint32)(buffer);
    uint16_t perm=(mode & (S_IRWXU | S_IRWXG | S_IRWXO));

    set_mode_system_stat(stat, perm); /* sftp uses the same permission bits as Linux */
    // logoutput_debug("read_attr_permissions_v04: perm %i", perm);
}

void read_attr_accesstime_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    int64_t sec=(* buffer->ops->rw.read.read_int64)(buffer);
    set_atime_sec_system_stat(stat, sec);
    // logoutput_debug("read_attr_accesstime_v04: sec %i", sec);
}

void read_attr_accesstime_n_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t nsec=(* buffer->ops->rw.read.read_uint32)(buffer);
    set_atime_nsec_system_stat(stat, nsec);
    // logoutput_debug("read_attr_accesstime_n_v04: nsec %i", nsec);
}

void read_attr_createtime_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    int64_t sec=(* buffer->ops->rw.read.read_int64)(buffer);
    set_btime_sec_system_stat(stat, sec);
}

void read_attr_createtime_n_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t nsec=(* buffer->ops->rw.read.read_uint32)(buffer);
    set_btime_nsec_system_stat(stat, nsec);
}

void read_attr_modifytime_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    int64_t sec=(* buffer->ops->rw.read.read_int64)(buffer);
    set_mtime_sec_system_stat(stat, sec);
    // logoutput_debug("read_attr_modifytime_v04: sec %i", sec);
}

void read_attr_modifytime_n_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t nsec=(* buffer->ops->rw.read.read_uint32)(buffer);
    set_mtime_nsec_system_stat(stat, nsec);
    // logoutput_debug("read_attr_modifytime_n_v04: nsec %i", nsec);
}

static void _attr_acl_string_cb(struct attr_buffer_s *buffer, struct ssh_string_s *aclblock, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;
    char *pos=aclblock->ptr;
    unsigned int left=aclblock->len;
    unsigned int ace_count=0;
    unsigned int ace_type=0;
    unsigned int ace_flag=0;
    unsigned int ace_mask=0;
    unsigned int len=0;

    /*

	See: https://tools.ietf.org/html/draft-ietf-secsh-filexfer-04#section-5.7

	acl's have the form:
	- uint32			ace-count
	- ACE				ace[ace-count]

	one ace looks like:
	- uint32			ace-type (like ALLOW, DENY, AUDIT and ALARM)
	- uint32			ace-flag
	- uint32			ace-mask (what)
	- string			who
    */

    ace_count=get_uint32(pos);
    pos+=4;
    left-=4;

    for (unsigned int i=0; i<ace_count; i++) {

	ace_type=get_uint32(pos);
	pos+=4;
	left-=4;

	ace_flag=get_uint32(pos);
	pos+=4;
	left-=4;

	ace_mask=get_uint32(pos);
	pos+=4;
	left-=4;

	/* who */

	len=get_uint32(pos);
	pos+=4;
	left-=4;

	/* do nothing now ... */

	pos+=len;
	left-=len;

    }

}

void read_attr_acl_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct _attr_cb_s _cb_data={.ctx=ctx, .r=r, .stat=stat};
    struct ssh_string_s acl=SSH_STRING_INIT;

    /* 20210708: do nothing with the acl for now, use a dummy acl string */

    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &acl, _attr_acl_string_cb, (void *) &_cb_data);
}

/*
    read a name and attributes from a name response
    for version 4 a name response looks like:

    uint32				id
    uint32				count
    repeats count times:
	string				filename [UTF-8]
	ATTRS				attr

*/

static void _dummy_cb(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
}

void read_name_name_response_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name)
{
    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, name, _dummy_cb, NULL);

    /* TODO: convert from UTF-8 to local */
}

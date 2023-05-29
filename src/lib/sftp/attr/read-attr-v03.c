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

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"
#include "sftp/attr-context.h"
#include "read-attr-v03.h"

static unsigned int type_mapping[5]={0, S_IFREG, S_IFDIR, S_IFLNK, 0};

void read_attr_zero(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
}

void read_attr_type_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned char tmp=(* buffer->ops->rw.read.read_uchar)(buffer);
    unsigned int type=(tmp<5) ? type_mapping[tmp] : 0;
    set_type_system_stat(stat, type);
}

void read_attr_size_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint64_t size=(* buffer->ops->rw.read.read_uint64)(buffer);
    set_size_system_stat(stat, size);
}

void read_attr_uidgid_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct net_entity_s user;
    struct net_entity_s group;

    /* uid */

    user.net.id=(* buffer->ops->rw.read.read_uint32)(buffer);
    (* actx->mapping->mapcb.get_user_p2l)(actx->mapping, &user);
    set_uid_system_stat(stat, user.localid);

    /* gid */

    group.net.id=(* buffer->ops->rw.read.read_uint32)(buffer);
    (* actx->mapping->mapcb.get_group_p2l)(actx->mapping, &group);
    set_gid_system_stat(stat, group.localid);

}

void read_attr_permissions_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned int mode=(* buffer->ops->rw.read.read_uint32)(buffer);
    uint16_t type=(mode & S_IFMT);
    uint16_t perm=(mode & (S_IRWXU | S_IRWXG | S_IRWXO));

    set_type_system_stat(stat, type);
    set_mode_system_stat(stat, perm); /* sftp uses the same permission bits as Linux */
}

void read_attr_acmodtime_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    /* access time */

    time.st_sec=(* buffer->ops->rw.read.read_uint32)(buffer);
    set_atime_system_stat(stat, &time);

    /* modify time */

    time.st_sec=(* buffer->ops->rw.read.read_uint32)(buffer);
    set_mtime_system_stat(stat, &time);

}

/* 20220821: for now do nothing with extended attributes, only read them */

static void _attr_cb_extension_dummy(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
}

void read_attr_extensions_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    uint32_t count=(* buffer->ops->rw.read.read_uint32)(buffer);
    unsigned int len=0;
    struct ssh_string_s name=SSH_STRING_INIT;
    struct ssh_string_s data=SSH_STRING_INIT;

    for (unsigned int i=0; i<count; i++) {

	len=(* buffer->ops->rw.read.read_string)(buffer, &name, _attr_cb_extension_dummy, NULL);
	len=(* buffer->ops->rw.read.read_string)(buffer, &data, _attr_cb_extension_dummy, NULL);

    }

}

/* read the name of a NAME response
    */

/*
    read a name and attributes from a name response
    for version 4 a name response looks like:

    uint32				id
    uint32				count
    repeats count times:
	string				filename
	string				longname
	ATTRS				attr


    longname is output of ls -l command like:

    -rwxr-xr-x   1 mjos     staff      348911 Mar 25 14:29 t-filexfer
    1234567890 123 12345678 12345678 12345678 123456789012
    01234567890123456789012345678901234567890123456789012345
    0         1         2         3         4         5

    example:

    -rw-------    1 sbon     sbon         1799 Nov 26 15:22 .bash_history


*/

static void _dummy_cb(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
}

void read_name_name_response_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name)
{
    struct ssh_string_s longname=SSH_STRING_INIT;
    uint32_t len=0;

    len=(* buffer->ops->rw.read.read_string)(buffer, name, _dummy_cb, NULL);

    /* longname, ignore */

    len=(* buffer->ops->rw.read.read_string)(buffer, &longname, _dummy_cb, NULL);
}


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

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"
#include "sftp/attr-context.h"

#include "read-attr-generic.h"
#include "read-attr-v03.h"

static unsigned int type_mapping[5]={0, S_IFREG, S_IFDIR, S_IFLNK, 0};

void read_attr_zero(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
}

void read_attr_size_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{

    attr->size=(* buffer->ops->rw.read.read_uint64)(buffer);

    attr->received |= SFTP_ATTR_SIZE;
    r->done |= SSH_FILEXFER_ATTR_SIZE;
    r->todo &= ~SSH_FILEXFER_ATTR_SIZE;

}

void read_attr_uidgid_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    struct sftp_user_s user;
    struct sftp_group_s group;

    /* uid */

    user.remote.id=(* buffer->ops->rw.read.read_uint32)(buffer);

    (* ctx->get_local_uid_byid)(ctx, &user);
    attr->user.uid=user.uid;
    attr->received |= SFTP_ATTR_USER;

    /* gid */

    group.remote.id=(* buffer->ops->rw.read.read_uint32)(buffer);

    (* ctx->get_local_gid_byid)(ctx, &group);
    attr->group.gid=group.gid;
    attr->received |= SFTP_ATTR_GROUP;

    r->done |= SSH_FILEXFER_ATTR_UIDGID;
    r->todo &= ~SSH_FILEXFER_ATTR_UIDGID;

    logoutput("read_attr_uidgid_v03: remote %i:%i local %i:%i", user.remote.id, group.remote.id, attr->user.uid, attr->group.gid);
}

void read_attr_permissions_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int perm=(* buffer->ops->rw.read.read_uint32)(buffer);

    attr->permissions=(S_IRWXU | S_IRWXG | S_IRWXO) & perm; /* sftp uses the same permission bits as Linux */
    attr->received |= SFTP_ATTR_PERMISSIONS;

    r->done |= SSH_FILEXFER_ATTR_PERMISSIONS;
    r->todo &= ~SSH_FILEXFER_ATTR_PERMISSIONS;

    logoutput("read_attr_permissions_v03");

}

void read_attr_acmodtime_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{

    /* access time */

    attr->atime=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->received |= SFTP_ATTR_ATIME;

    /* modify time */

    attr->mtime=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->received |= SFTP_ATTR_MTIME;

    r->done |= SSH_FILEXFER_ATTR_ACMODTIME;
    r->todo &= ~SSH_FILEXFER_ATTR_ACMODTIME;

}

void read_attr_extensions_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    /* what to do here ? */
    r->done |= SSH_FILEXFER_ATTR_EXTENDED;
    r->todo &= ~SSH_FILEXFER_ATTR_EXTENDED;
}

static struct _rw_attrcb_s read_attr03[] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}},
		    {SSH_FILEXFER_ATTR_UIDGID,			1,				{read_attr_zero, read_attr_uidgid_v03}},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03}},
		    {SSH_FILEXFER_ATTR_ACMODTIME, 		3,				{read_attr_zero, read_attr_acmodtime_v03}},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03}}};


void read_sftp_attributes_v03(struct attr_context_s *ctx, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct rw_attr_result_s r;

    memset(&r, 0, sizeof(struct rw_attr_result_s));
    r.valid=valid;
    r.todo=valid;
    r.attrcb=read_attr03;
    r.count=5;

    read_sftp_attributes_generic(ctx, buffer, &r, attr);

}

void read_attributes_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    read_sftp_attributes_v03(ctx, valid, buffer, attr);
}

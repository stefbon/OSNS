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

#include "write-attr-generic.h"
#include "write-attr-v03.h"

static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, 0, 0, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, 0, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, 0};

void write_attr_zero(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
}

void write_attr_size_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{

    (* buffer->ops->rw.write.write_uint64)(buffer, attr->size);

    r->done |= SSH_FILEXFER_ATTR_SIZE;
    r->todo &= ~SSH_FILEXFER_ATTR_SIZE;
}

void write_attr_uidgid_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    struct sftp_user_s user;
    struct sftp_group_s group;

    user.uid=attr->user.uid;
    (* ctx->get_remote_uid_byid)(ctx, &user);
    (* buffer->ops->rw.write.write_uint32)(buffer, user.remote.id);

    group.gid=attr->group.gid;
    (* ctx->get_remote_gid_byid)(ctx, &group);
    (* buffer->ops->rw.write.write_uint32)(buffer, group.remote.id);

    r->done |= SSH_FILEXFER_ATTR_UIDGID;
    r->todo &= ~SSH_FILEXFER_ATTR_UIDGID;

}

void write_attr_permissions_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->permissions & ( S_IRWXU | S_IRWXG | S_IRWXO ));

    r->done |= SSH_FILEXFER_ATTR_PERMISSIONS;
    r->todo &= ~SSH_FILEXFER_ATTR_PERMISSIONS;
}

void write_attr_acmodtime_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->atime);
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->mtime);

    r->done |= SSH_FILEXFER_ATTR_ACMODTIME;
    r->todo &= ~SSH_FILEXFER_ATTR_ACMODTIME;
}

static struct _rw_attrcb_s write_attr_acb[] = {
	{SSH_FILEXFER_ATTR_SIZE, 		0,		{write_attr_zero, write_attr_size_v03}},
	{SSH_FILEXFER_ATTR_UIDGID, 		1,		{write_attr_zero, write_attr_uidgid_v03}},
	{SSH_FILEXFER_ATTR_PERMISSIONS, 	2,		{write_attr_zero, write_attr_permissions_v03}},
	{SSH_FILEXFER_ATTR_ACMODTIME, 		3,		{write_attr_zero, write_attr_acmodtime_v03}}};

void write_attributes_v03(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int type=0;

    r->attrcb=write_attr_acb;
    r->count=4;

    (* buffer->ops->rw.write.write_uint32)(buffer, r->valid);
    type=IFTODT(attr->type);
    (* buffer->ops->rw.write.write_uchar)(buffer, (type<13) ? type_reverse[type] : (unsigned char) SSH_FILEXFER_TYPE_UNKNOWN);

    write_sftp_attributes_generic(ctx, buffer, r, attr);

}

unsigned int write_attributes_len_v03(struct attr_context_s *ctx, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned char *vb=r->vb;
    unsigned int len=0;

    memset(r, 0, sizeof(struct rw_attr_result_s));

    vb[WRITE_ATTR_VB_SIZE] = (unsigned char) (attr->asked && SFTP_ATTR_SIZE);
    vb[WRITE_ATTR_VB_UIDGID] = (unsigned char) (attr->asked && (SFTP_ATTR_USER | SFTP_ATTR_GROUP));
    vb[WRITE_ATTR_VB_PERMISSIONS] = (unsigned char) (attr->asked && SFTP_ATTR_PERMISSIONS);
    vb[WRITE_ATTR_VB_ACMODTIME] = (unsigned char) (attr->asked && (SFTP_ATTR_ATIME | SFTP_ATTR_MTIME));

    len = 5; 		/* valid flag (32 bits) plus type byte */
    len += 8 * vb[WRITE_ATTR_VB_SIZE]; 	/* size  (8 bytes) */
    len += 8 * vb[WRITE_ATTR_VB_UIDGID]; 	/* user and/or group using uid and gid */
    len += 4 * vb[WRITE_ATTR_VB_PERMISSIONS]; 	/* permissions */
    len += 8 * vb[WRITE_ATTR_VB_ACMODTIME]; 	/* ac mod time */

    r->todo = 	(vb[WRITE_ATTR_VB_SIZE] * SSH_FILEXFER_ATTR_SIZE +
		vb[WRITE_ATTR_VB_UIDGID] * SSH_FILEXFER_ATTR_UIDGID +
		vb[WRITE_ATTR_VB_PERMISSIONS] * SSH_FILEXFER_ATTR_PERMISSIONS +
		vb[WRITE_ATTR_VB_ACMODTIME] * SSH_FILEXFER_ATTR_ACMODTIME);

    r->valid = r->todo;

    return len;

}

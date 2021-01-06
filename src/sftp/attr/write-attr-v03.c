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

#include "logging.h"
#include "main.h"

#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"
#include "write-attr-v03.h"
#include "datatypes/ssh-uint.h"

void write_attr_zero(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
}

void write_attr_size_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    store_uint64(buffer->pos, attr->size);
    buffer->pos+=8;
    *valid|=SSH_FILEXFER_ATTR_SIZE;
}

void write_attr_uidgid_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;
    struct sftp_user_s user;
    struct sftp_group_s group;

    user.uid=attr->user.uid;
    (* usermapping->get_remote_user)(sftp, &user);
    (* buffer->ops->rw.write.write_uint32)(buffer, user.remote.id);

    group.gid=attr->group.gid;
    (* usermapping->get_remote_group)(sftp, &group);
    (* buffer->ops->rw.write.write_uint32)(buffer, group.remote.id);

    *valid|=SSH_FILEXFER_ATTR_UIDGID;

}

void write_attr_permissions_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->permissions & ( S_IRWXU | S_IRWXG | S_IRWXO ));
    *valid|=SSH_FILEXFER_ATTR_PERMISSIONS;
}

void write_attr_acmodtime_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->atime);
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->mtime);
    *valid|=SSH_FILEXFER_ATTR_ACMODTIME;
}

static write_attr_cb write_attr_acb[][2] = {
	{write_attr_zero, write_attr_size_v03},
	{write_attr_zero, write_attr_uidgid_v03},
	{write_attr_zero, write_attr_permissions_v03},
	{write_attr_zero, write_attr_acmodtime_v03}};

void write_attributes_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned char vb[4];
    unsigned int valid=0;
    char *pos=NULL;

    vb[0] = (attr->asked && SFTP_ATTR_SIZE);
    vb[1] = (attr->asked && (SFTP_ATTR_USER | SFTP_ATTR_GROUP));
    vb[2] = (attr->asked && SFTP_ATTR_PERMISSIONS);
    vb[3] = (attr->asked && (SFTP_ATTR_ATIME | SFTP_ATTR_MTIME));

    pos=buffer->pos;
    (* buffer->ops->rw.write.write_uint32)(buffer, valid); /* correct this later */

    /* size */

    (* write_attr_acb[0][vb[0]]) (sftp, buffer, attr, &valid);

    /* owner and/or group */

    (* write_attr_acb[1][vb[1]]) (sftp, buffer, attr, &valid);

    /* permissions */

    (* write_attr_acb[2][vb[2]]) (sftp, buffer, attr, &valid);

    /* acmod time */

    (* write_attr_acb[3][vb[3]]) (sftp, buffer, attr, &valid);

    /* valid is set: write it at begin */

    store_uint32(pos, valid);

}

unsigned int write_attributes_len_v03(struct sftp_client_s *sftp, struct sftp_attr_s *attr)
{
    unsigned char vb[4];
    unsigned int len=0;

    vb[0] = (attr->asked && SFTP_ATTR_SIZE);
    vb[1] = (attr->asked && (SFTP_ATTR_USER | SFTP_ATTR_GROUP));
    vb[2] = (attr->asked && SFTP_ATTR_PERMISSIONS);
    vb[3] = (attr->asked && (SFTP_ATTR_ATIME | SFTP_ATTR_MTIME));

    len = 4; 		/* valid flag */
    len += 8 * vb[0]; 	/* size */
    len += 8 * vb[1]; 	/* user and/or group using uid and gid */
    len += 4 * vb[2]; 	/* permissions */
    len += 8 * vb[3]; 	/* ac mod time */

    return len;

}

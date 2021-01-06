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

#include "workspace-interface.h"
#include "ssh-common.h"
#include "ssh-channel.h"
#include "ssh-utils.h"
#include "ssh-hostinfo.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v06.h"
#include "write-attr-v03.h"
#include "write-attr-v04.h"
#include "write-attr-v05.h"
#include "write-attr-v06.h"

static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_FIFO, SSH_FILEXFER_TYPE_CHAR_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_BLOCK_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SOCKET};

void write_attr_accesstime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    (* buffer->ops->rw.write.write_uint64)(buffer, attr->atime);
    *valid|=SSH_FILEXFER_ATTR_ACCESSTIME;

    if (sftp->supported.version.v06.attribute_mask & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) {

	(* buffer->ops->rw.write.write_uint32)(buffer, attr->atime_n);
	*valid|=SSH_FILEXFER_ATTR_SUBSECOND_TIMES;

    }

}

void write_attr_modifytime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    (* buffer->ops->rw.write.write_uint64)(buffer, attr->mtime);
    *valid|=SSH_FILEXFER_ATTR_MODIFYTIME;

    if (sftp->supported.version.v06.attribute_mask & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) {

	(* buffer->ops->rw.write.write_uint32)(buffer, attr->ctime_n);
	*valid|=SSH_FILEXFER_ATTR_SUBSECOND_TIMES;

    }

}

void write_attr_changetime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid)
{
    (* buffer->ops->rw.write.write_uint64)(buffer, attr->ctime);
    *valid|=SSH_FILEXFER_ATTR_CTIME;

    if (sftp->supported.version.v06.attribute_mask & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) {

	(* buffer->ops->rw.write.write_uint32)(buffer, attr->ctime_n);
	*valid|=SSH_FILEXFER_ATTR_SUBSECOND_TIMES;

    }

}

static write_attr_cb write_attr_acb[][2] = {
	{write_attr_zero, write_attr_size_v03},
	{write_attr_zero, write_attr_ownergroup_v04},
	{write_attr_zero, write_attr_permissions_v03},
	{write_attr_zero, write_attr_accesstime_v06},
	{write_attr_zero, write_attr_modifytime_v06},
	{write_attr_zero, write_attr_changetime_v06}};

void write_attributes_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned char vb[6];
    unsigned int valid=0;
    char *pos=NULL;
    unsigned int type=0;

    vb[0] = (attr->asked && SFTP_ATTR_SIZE);
    vb[1] = (attr->asked && (SFTP_ATTR_USER | SFTP_ATTR_GROUP));
    vb[2] = (attr->asked && SFTP_ATTR_PERMISSIONS);
    vb[3] = (attr->asked && SFTP_ATTR_ATIME);
    vb[4] = (attr->asked && SFTP_ATTR_MTIME);
    vb[5] = (attr->asked && SFTP_ATTR_CTIME);

    pos=buffer->pos;
    (* buffer->ops->rw.write.write_uint32)(buffer, valid); /* correct this later */

    type=IFTODT(attr->type);
    (* buffer->ops->rw.write.write_uchar)(buffer, (type<13) ? type_reverse[type] : (unsigned char) SSH_FILEXFER_TYPE_UNKNOWN);

    /* size */

    (* write_attr_acb[0][vb[0]]) (sftp, buffer, attr, &valid);

    /* owner and/or group */

    (* write_attr_acb[1][vb[1]]) (sftp, buffer, attr, &valid);

    /* permissions */

    (* write_attr_acb[2][vb[2]]) (sftp, buffer, attr, &valid);

    /* access time */

    (* write_attr_acb[3][vb[3]]) (sftp, buffer, attr, &valid);

    /* modify time */

    (* write_attr_acb[4][vb[4]]) (sftp, buffer, attr, &valid);

    /* change time */

    (* write_attr_acb[5][vb[5]]) (sftp, buffer, attr, &valid);

    /* valid is set: write it at begin */

    store_uint32(pos, valid);

}

unsigned int write_attributes_len_v06(struct sftp_client_s *sftp, struct sftp_attr_s *attr)
{
    unsigned char vb[6];
    unsigned int len=0;

    vb[0] = (attr->asked && SFTP_ATTR_SIZE);
    vb[1] = (attr->asked && (SFTP_ATTR_USER | SFTP_ATTR_GROUP));
    vb[2] = (attr->asked && SFTP_ATTR_PERMISSIONS);
    vb[3] = (attr->asked && SFTP_ATTR_ATIME);
    vb[4] = (attr->asked && SFTP_ATTR_MTIME);
    vb[5] = (attr->asked && SFTP_ATTR_CTIME);

    /* determine the required length */

    len = 5; /* valid flag + type byte */
    len += vb[0] * 8; /* size */

    if (attr->asked & SFTP_ATTR_USER) {
	struct sftp_user_s user;

	user.remote.name.ptr=NULL;
	user.remote.name.len=0;
	user.uid=attr->user.uid;
	(* sftp->usermapping.get_remote_user)(sftp, &user);

	len+=4 + user.remote.name.len;

    }

    if (attr->asked & SFTP_ATTR_GROUP) {
	struct sftp_group_s group;

	group.remote.name.ptr=NULL;
	group.remote.name.len=0;
	group.gid=attr->group.gid;
	(* sftp->usermapping.get_remote_group)(sftp, &group);

	len+=4 + group.remote.name.len;

    }

    len += vb[2] * 4; /* permissions */
    len += vb[3] * 12; /* access time (8+4)*/
    len += vb[4] * 12; /* modify time (8+4)*/
    len += vb[5] * 12; /* change time (8+4)*/

    return len;

}

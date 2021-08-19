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

#include "log.h"
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

#include "sftp/attr-context.h"
#include "write-attr-generic.h"
#include "write-attr-v03.h"
#include "write-attr-v04.h"
#include "write-attr-v05.h"

static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_FIFO, SSH_FILEXFER_TYPE_CHAR_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_BLOCK_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SOCKET};

void write_attr_changetime_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{

    (* buffer->ops->rw.write.write_uint64)(buffer, attr->ctime);

    r->done |= SSH_FILEXFER_ATTR_CTIME;
    r->todo &= ~SSH_FILEXFER_ATTR_CTIME;

    (* r->ntimecb[WRITE_ATTR_NT_CTIME].cb[r->nt[WRITE_ATTR_NT_CTIME]])(ctx, buffer, r, attr);

}

void write_attr_changetime_n_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    (* buffer->ops->rw.write.write_uint32)(buffer, attr->ctime_n);
    r->done |= SSH_FILEXFER_ATTR_SUBSECOND_TIMES;
}

static struct _rw_attrcb_s write_attr_acb[] = {
	{SSH_FILEXFER_ATTR_SIZE, 		0,		{write_attr_zero, write_attr_size_v03}},
	{SSH_FILEXFER_ATTR_OWNERGROUP, 		7,		{write_attr_zero, write_attr_ownergroup_v04}},
	{SSH_FILEXFER_ATTR_PERMISSIONS, 	2,		{write_attr_zero, write_attr_permissions_v03}},
	{SSH_FILEXFER_ATTR_ACCESSTIME, 		3,		{write_attr_zero, write_attr_accesstime_v04}},
	{SSH_FILEXFER_ATTR_MODIFYTIME, 		5,		{write_attr_zero, write_attr_modifytime_v04}},
	{SSH_FILEXFER_ATTR_CTIME, 		5,		{write_attr_zero, write_attr_changetime_v06}},
	};


static struct _rw_attrcb_s write_attr_ant[] = {
	{SSH_FILEXFER_ATTR_SUBSECOND_TIMES,	8,		{write_attr_zero, write_attr_accesstime_n_v04}},
	{SSH_FILEXFER_ATTR_SUBSECOND_TIMES,	8,		{write_attr_zero, write_attr_modifytime_n_v04}},
	{SSH_FILEXFER_ATTR_SUBSECOND_TIMES,	8,		{write_attr_zero, write_attr_changetime_n_v06}},
	};


void write_attributes_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int type=0;

    r->attrcb=write_attr_acb;
    r->count=6;
    r->ntimecb=write_attr_ant;

    (* buffer->ops->rw.write.write_uint32)(buffer, r->valid);
    type=IFTODT(attr->type);
    (* buffer->ops->rw.write.write_uchar)(buffer, (type<13) ? type_reverse[type] : (unsigned char) SSH_FILEXFER_TYPE_UNKNOWN);

    write_sftp_attributes_generic(ctx, buffer, r, attr);

}

unsigned int write_attributes_len_v06(struct attr_context_s *ctx, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned char *vb=r->vb;
    unsigned char *nt=r->nt;
    unsigned int len=0;

    memset(r, 0, sizeof(struct rw_attr_result_s));

    vb[WRITE_ATTR_VB_SIZE] = (unsigned char) (attr->asked && SFTP_ATTR_SIZE);
    vb[WRITE_ATTR_VB_PERMISSIONS] = (unsigned char) (attr->asked && SFTP_ATTR_PERMISSIONS);
    vb[WRITE_ATTR_VB_ATIME] = (unsigned char) (attr->asked && SFTP_ATTR_ATIME);
    vb[WRITE_ATTR_VB_MTIME] = (unsigned char) (attr->asked && SFTP_ATTR_MTIME);
    vb[WRITE_ATTR_VB_CTIME] = (unsigned char) (attr->asked && SFTP_ATTR_CTIME);

    /* write nanoseconds only if times are required
	TODO: make this configurable ... ??? */

    nt[WRITE_ATTR_NT_ATIME] = vb[WRITE_ATTR_VB_ATIME];
    nt[WRITE_ATTR_NT_MTIME] = vb[WRITE_ATTR_VB_MTIME];
    nt[WRITE_ATTR_NT_CTIME] = vb[WRITE_ATTR_VB_CTIME];

    len = 5; /* valid flag + type byte */
    len += vb[0] * 8; /* size */

    r->todo = 	(vb[WRITE_ATTR_VB_SIZE] * SSH_FILEXFER_ATTR_SIZE +
		vb[WRITE_ATTR_VB_PERMISSIONS] * SSH_FILEXFER_ATTR_PERMISSIONS +
		vb[WRITE_ATTR_VB_ATIME] * SSH_FILEXFER_ATTR_ACCESSTIME +
		vb[WRITE_ATTR_VB_MTIME] * SSH_FILEXFER_ATTR_MODIFYTIME +
		vb[WRITE_ATTR_VB_CTIME] * SSH_FILEXFER_ATTR_CTIME);

    r->valid = r->todo + ((vb[WRITE_ATTR_VB_ATIME] | vb[WRITE_ATTR_VB_MTIME] | vb[WRITE_ATTR_VB_CTIME]) * SSH_FILEXFER_ATTR_SUBSECOND_TIMES);

    /* get functions to get the maximum size of the owner and group fields:
	user@domain
	user is maximal 255
	domain is maximal ? */

    if (attr->asked & SFTP_ATTR_USER) {

	len += (* ctx->maxlength_username)(ctx) + 5 + (* ctx->maxlength_domainname)(ctx);
	vb[WRITE_ATTR_VB_OWNERGROUP] = 1;
	r->valid |= SSH_FILEXFER_ATTR_OWNERGROUP;

    }

    if (attr->asked & SFTP_ATTR_GROUP) {

	len += (* ctx->maxlength_groupname)(ctx) + 5 + (* ctx->maxlength_domainname)(ctx);
	vb[WRITE_ATTR_VB_OWNERGROUP] = 1;
	r->valid |= SSH_FILEXFER_ATTR_OWNERGROUP;

    }

    len += vb[WRITE_ATTR_VB_PERMISSIONS] * 4;
    len += vb[WRITE_ATTR_VB_ATIME] * 8 + nt[WRITE_ATTR_NT_ATIME] * 4 ;
    len += vb[WRITE_ATTR_VB_MTIME] * 8 + nt[WRITE_ATTR_NT_MTIME] * 4 ;
    len += vb[WRITE_ATTR_VB_CTIME] * 8 + nt[WRITE_ATTR_NT_CTIME] * 4 ;

    return len;

}

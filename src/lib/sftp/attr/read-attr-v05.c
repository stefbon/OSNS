/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "sftp/protocol-v05.h"
#include "sftp/attr-context.h"

#include "read-attr-generic.h"
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "read-attr-v05.h"

#include "datatypes/ssh-uint.h"

/* more information:

   https://tools.ietf.org/html/draft-ietf-secsh-filexfer-05#section-5 */

static unsigned int type_mapping[10]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0, S_IFSOCK, S_IFCHR, S_IFBLK, S_IFIFO};
static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_FIFO, SSH_FILEXFER_TYPE_CHAR_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_BLOCK_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SOCKET};

void read_attr_bits_v05(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int bits=(* buffer->ops->rw.read.read_uint32)(buffer);

    r->done &= ~SSH_FILEXFER_ATTR_BITS;
    r->done &= ~SSH_FILEXFER_ATTR_BITS;
}

static struct _rw_attrcb_s read_attr05[] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}},
		    {SSH_FILEXFER_ATTR_OWNERGROUP,		7,				{read_attr_zero, read_attr_ownergroup_v04}},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03}},
		    {SSH_FILEXFER_ATTR_ACCESSTIME, 		3,				{read_attr_zero, read_attr_accesstime_v04}},
		    {SSH_FILEXFER_ATTR_CREATETIME, 		4,				{read_attr_zero, read_attr_createtime_v04}},
		    {SSH_FILEXFER_ATTR_MODIFYTIME,		5,				{read_attr_zero, read_attr_modifytime_v04}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_subsecond_time_v04}},
		    {SSH_FILEXFER_ATTR_ACL,			6,				{read_attr_zero, read_attr_acl_v04}},
		    {SSH_FILEXFER_ATTR_BITS,			9,				{read_attr_zero, read_attr_bits_v05}},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03}}};

static struct _rw_attrcb_s read_ntime05[] = {
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_createtime_n_v04}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_modifytime_n_v04}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_accesstime_n_v04}}};


static void read_sftp_attributes(struct attr_context_s *ctx, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct rw_attr_result_s r;
    unsigned char type=0;

    memset(&r, 0, sizeof(struct rw_attr_result_s));
    r.valid=valid;
    r.todo=(valid & ~SSH_FILEXFER_ATTR_SUBSECOND_TIMES);
    r.attrcb=read_attr05;
    r.count=10;
    r.ntimecb=read_ntime05;

    type=(* buffer->ops->rw.read.read_uchar)(buffer);
    attr->type=(type<10) ? type_mapping[type] : 0;
    attr->received|=SFTP_ATTR_TYPE;

    read_sftp_attributes_generic(ctx, buffer, &r, attr);
}

void read_attributes_v05(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    read_sftp_attributes(ctx, valid, buffer, attr);
}

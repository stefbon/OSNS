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

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v06.h"
#include "sftp/attr-context.h"

#include "read-attr-generic.h"
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "read-attr-v05.h"
#include "read-attr-v06.h"

#include "datatypes/ssh-uint.h"

static unsigned int type_mapping[10]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0, S_IFSOCK, S_IFCHR, S_IFBLK, S_IFIFO};

void read_attr_alloc_size_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    /* 20210708: do nothing with the alloc size, use a dummy variable to store it */
    uint64_t alloc_size=(* buffer->ops->rw.read.read_uint64)(buffer);

    r->done |= SSH_FILEXFER_ATTR_ALLOCATION_SIZE;
    r->todo &= ~SSH_FILEXFER_ATTR_ALLOCATION_SIZE;
}

void read_attr_modifytime_n_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    attr->mtime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
}

void read_attr_changetime_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    struct _rw_attrcb_s *ntimecb=r->ntimecb;
    unsigned char flag=0;

    attr->ctime=(* buffer->ops->rw.read.read_uint64)(buffer);
    attr->received|=SFTP_ATTR_CTIME;

    r->done |= SSH_FILEXFER_ATTR_CTIME;
    r->todo &= ~SSH_FILEXFER_ATTR_CTIME;

    flag=(r->valid & ntimecb[WRITE_ATTR_NT_CTIME].code) >> ntimecb[WRITE_ATTR_NT_CTIME].shift;
    (* ntimecb[WRITE_ATTR_NT_CTIME].cb[flag])(ctx, buffer, r, attr);

}

void read_attr_changetime_n_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    attr->ctime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
}

struct _attr_cb_s {
    struct attr_context_s 		*ctx;
    struct rw_attr_result_s  		*r;
    struct sftp_attr_s 			*attr;
};

static void _attr_acl_string_cb(struct attr_buffer_s *buffer, struct ssh_string_s *aclblock, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;
    char *pos=aclblock->ptr;
    unsigned int left=aclblock->len;
    unsigned int ace_flags=0;
    unsigned int ace_count=0;
    unsigned int ace_type=0;
    unsigned int ace_flag=0;
    unsigned int ace_mask=0;
    unsigned int len=0;

    /*

	See: https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13#section-7.8

	acl's have the form:
	- uint32			ace_flags
	- uint32			ace-count
	- ACE				ace[ace-count]

	one ace looks like:
	- uint32			ace-type (like ALLOW, DENY, AUDIT and ALARM)
	- uint32			ace-flag
	- uint32			ace-mask (what)
	- string			who

	this differs from the previous version:
	- added the first uint32 field ac_flags

	Note:
	- ace means access control entry. The NFS ACL attribute is an array of these.
    */

    ace_count=get_uint32(pos);
    pos+=4;
    left-=4;

    for (unsigned int i=0; i<ace_count; i++) {

	/* type of ace */

	ace_type=get_uint32(pos);
	pos+=4;
	left-=4;

	ace_flag=get_uint32(pos);
	pos+=4;
	left-=4;

	/* action */

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

void read_attr_acl_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.ctx=ctx, .r=r, .attr=attr};
    struct ssh_string_s acl=SSH_STRING_INIT;

    /* 20210708: do nothing with the acl for now, use a dummy acl string */

    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &acl, _attr_acl_string_cb, (void *) &_cb_data);

    r->done |= SSH_FILEXFER_ATTR_ACL;
    r->todo &= ~SSH_FILEXFER_ATTR_ACL;
}

void read_attr_bits_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int bits=(* buffer->ops->rw.read.read_uint32)(buffer);
    unsigned int bits_valid=(* buffer->ops->rw.read.read_uint32)(buffer);

    r->done |= SSH_FILEXFER_ATTR_BITS;
    r->todo &= ~SSH_FILEXFER_ATTR_BITS;
}

void read_attr_texthint_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned char texthint=(* buffer->ops->rw.read.read_uchar)(buffer);

    r->done |= SSH_FILEXFER_ATTR_TEXT_HINT;
    r->todo &= ~SSH_FILEXFER_ATTR_TEXT_HINT;
}

static void _attr_cb_mimetype(struct attr_buffer_s *buffer, struct ssh_string_s *mime, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;

    /* what here? lookup the mimetype in the local mime db, and get a code, and store that value somewhere in inode->stat/st/attr */
}

void read_attr_mimetype_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.ctx=ctx, .r=r, .attr=attr};
    struct ssh_string_s mimetype=SSH_STRING_INIT;

    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &mimetype, _attr_cb_mimetype, (void *) &_cb_data);

    r->done |= SSH_FILEXFER_ATTR_MIME_TYPE;
    r->todo &= ~SSH_FILEXFER_ATTR_MIME_TYPE;
}

void read_attr_link_count_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    unsigned int link_count=(* buffer->ops->rw.read.read_uint32)(buffer);

    r->done |= SSH_FILEXFER_ATTR_LINK_COUNT;
    r->todo &= ~SSH_FILEXFER_ATTR_LINK_COUNT;
}

static void _attr_cb_untrans_name(struct attr_buffer_s *buffer, struct ssh_string_s *name, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;

    /* what here? */
}

void read_attr_untranslated_name_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.ctx=ctx, .r=r, .attr=attr};
    struct ssh_string_s untrans_name=SSH_STRING_INIT;

    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &untrans_name, _attr_cb_untrans_name, (void *) &_cb_data);

    r->done |= SSH_FILEXFER_ATTR_UNTRANSLATED_NAME;
    r->todo &= ~SSH_FILEXFER_ATTR_UNTRANSLATED_NAME;
}

static struct _rw_attrcb_s read_attr06[] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}},
		    {SSH_FILEXFER_ATTR_ALLOCATION_SIZE, 	10,				{read_attr_zero, read_attr_alloc_size_v06}},
		    {SSH_FILEXFER_ATTR_OWNERGROUP,		7,				{read_attr_zero, read_attr_ownergroup_v04}},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03}},
		    {SSH_FILEXFER_ATTR_ACCESSTIME, 		3,				{read_attr_zero, read_attr_accesstime_v04}},
		    {SSH_FILEXFER_ATTR_CREATETIME, 		4,				{read_attr_zero, read_attr_createtime_v04}},
		    {SSH_FILEXFER_ATTR_MODIFYTIME,		5,				{read_attr_zero, read_attr_modifytime_v04}},
		    {SSH_FILEXFER_ATTR_CTIME,			15,				{read_attr_zero, read_attr_changetime_v06}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_subsecond_time_v04}},
		    {SSH_FILEXFER_ATTR_ACL,			6,				{read_attr_zero, read_attr_acl_v06}},
		    {SSH_FILEXFER_ATTR_BITS,			9,				{read_attr_zero, read_attr_bits_v06}},
		    {SSH_FILEXFER_ATTR_TEXT_HINT,		11,				{read_attr_zero, read_attr_texthint_v06}},
		    {SSH_FILEXFER_ATTR_MIME_TYPE,		12,				{read_attr_zero, read_attr_mimetype_v06}},
		    {SSH_FILEXFER_ATTR_LINK_COUNT,		13,				{read_attr_zero, read_attr_link_count_v06}},
		    {SSH_FILEXFER_ATTR_UNTRANSLATED_NAME,	14,				{read_attr_zero, read_attr_untranslated_name_v06}},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03}}};

static struct _rw_attrcb_s read_ntime06[] = {
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_accesstime_n_v04}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_createtime_n_v04}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_modifytime_n_v06}},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_changetime_n_v06}}};



static void read_sftp_attributes(struct attr_context_s *ctx, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct rw_attr_result_s r;
    unsigned char type=0;

    logoutput("read_sftp_attributes (v06): valid %i", valid);

    memset(&r, 0, sizeof(struct rw_attr_result_s));
    r.valid=valid;
    r.todo=(valid & ~SSH_FILEXFER_ATTR_SUBSECOND_TIMES);
    r.attrcb=read_attr06;
    r.count=16;
    r.ntimecb=read_ntime06;

    type=(* buffer->ops->rw.read.read_uchar)(buffer);
    attr->type=(type<10) ? type_mapping[type] : 0;
    attr->received|=SFTP_ATTR_TYPE;

    read_sftp_attributes_generic(ctx, buffer, &r, attr);

}

void read_attributes_v06(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    read_sftp_attributes(ctx, valid, buffer, attr);
}


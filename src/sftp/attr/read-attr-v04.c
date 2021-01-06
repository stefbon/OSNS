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
#include "utils.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v04.h"
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "datatypes/ssh-uint.h"

static unsigned int type_mapping[6]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0};

struct _attr_cb_s {
    struct sftp_client_s 		*sftp;
    struct attr_version_s 		*av;
    struct sftp_attr_s 			*attr;
};

static void _attr_cb_user(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;
    struct sftp_client_s *sftp=data->sftp;
    struct attr_version_s *av=data->av;
    struct sftp_attr_s *attr=data->attr;
    struct sftp_user_s user;

    user.remote.name.ptr=av->version.v46.owner.ptr;
    user.remote.name.len=av->version.v46.owner.len;

    logoutput("_attr_cb_user: %.*s pos %i", user.remote.name.len, user.remote.name.ptr, (int)(buffer->pos - buffer->buffer));
    (* sftp->usermapping.get_local_uid)(sftp, &user);

    attr->user.uid=user.uid;
    attr->received|=SFTP_ATTR_USER;

}

static void _attr_cb_group(struct attr_buffer_s *buffer, struct ssh_string_s *s, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;
    struct sftp_client_s *sftp=data->sftp;
    struct attr_version_s *av=data->av;
    struct sftp_attr_s *attr=data->attr;
    struct sftp_group_s group;

    group.remote.name.ptr=av->version.v46.group.ptr;
    group.remote.name.len=av->version.v46.group.len;

    logoutput("_attr_cb_group: %.*s pos %i", group.remote.name.len, group.remote.name.ptr, (int)(buffer->pos - buffer->buffer));
    (* sftp->usermapping.get_local_gid)(sftp, &group);

    attr->group.gid=group.gid;
    attr->received|=SFTP_ATTR_GROUP;

}

void read_attr_ownergroup_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.sftp=sftp, .av=av04, .attr=attr};
    uint32_t len=0;

    len=(* buffer->ops->rw.read.read_string)(buffer, &av04->version.v46.owner, _attr_cb_user, (void *) &_cb_data);
    len=(* buffer->ops->rw.read.read_string)(buffer, &av04->version.v46.group, _attr_cb_group, (void *) &_cb_data);
    av04->valid -= SSH_FILEXFER_ATTR_OWNERGROUP;
}

void read_attr_accesstime_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    struct _valid_attrcb_s *ntimecb=av04->ntimecb;
    unsigned char flag=0;

    av04->version.v46.accesstime=(* buffer->ops->rw.read.read_uint64)(buffer);
    attr->atime=av04->version.v46.accesstime;
    attr->received|=SFTP_ATTR_ATIME;
    av04->valid -= SSH_FILEXFER_ATTR_ACCESSTIME;

    flag=(av04->valid & ntimecb[1].code) >> ntimecb[1].shift;
    (* ntimecb[1].cb[flag])(sftp, buffer, av04, attr);

}

void read_attr_accesstime_n_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    av04->version.v46.accesstime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->atime_n=av04->version.v46.accesstime_n;
}

void read_attr_createtime_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    struct _valid_attrcb_s *ntimecb=av04->ntimecb;
    unsigned char flag=0;

    av04->version.v46.createtime=(* buffer->ops->rw.read.read_uint64)(buffer);
    av04->valid -= SSH_FILEXFER_ATTR_CREATETIME;

    flag=(av04->valid & ntimecb[2].code) >> ntimecb[2].shift;
    (* ntimecb[2].cb[flag])(sftp, buffer, av04, attr);

}

void read_attr_createtime_n_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    av04->version.v46.createtime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
}

void read_attr_modifytime_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    struct _valid_attrcb_s *ntimecb=av04->ntimecb;
    unsigned char flag=0;

    av04->version.v46.modifytime=(* buffer->ops->rw.read.read_uint64)(buffer);
    attr->mtime=av04->version.v46.modifytime;
    attr->received|=SFTP_ATTR_MTIME;
    av04->valid -= SSH_FILEXFER_ATTR_MODIFYTIME;

    flag=(av04->valid & ntimecb[2].code) >> ntimecb[3].shift;
    (* ntimecb[3].cb[flag])(sftp, buffer, av04, attr);

}

void read_attr_modifytime_n_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    struct _valid_attrcb_s *ntimecb=av04->ntimecb;

    av04->version.v46.modifytime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->mtime_n=av04->version.v46.modifytime_n;

    /* NOTE: this is the latest subseconds call for this protocol version
	so it's safe to unset this flag in valid, not too soon cause the SUBSECONDS
	flag is used more than once */
    av04->valid -= SSH_FILEXFER_ATTR_SUBSECOND_TIMES;

}

static void _attr_cb_acl(struct attr_buffer_s *buffer, struct ssh_string_s *aclblock, void *ptr)
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

void read_attr_acl_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.sftp=sftp, .av=av04, .attr=attr};
    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &av04->version.v46.acl, _attr_cb_acl, (void *) &_cb_data);
    av04->valid -= SSH_FILEXFER_ATTR_ACL;
}

static struct _valid_attrcb_s valid_attr04[] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}, 			"size"},
		    {SSH_FILEXFER_ATTR_OWNERGROUP,		7,				{read_attr_zero, read_attr_ownergroup_v04},		"ownergroup"},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03},		"permissions"},
		    {SSH_FILEXFER_ATTR_ACCESSTIME, 		3,				{read_attr_zero, read_attr_accesstime_v04},		"accesstime"},
		    {SSH_FILEXFER_ATTR_CREATETIME, 		4,				{read_attr_zero, read_attr_createtime_v04},		"createtime"},
		    {SSH_FILEXFER_ATTR_MODIFYTIME,		5,				{read_attr_zero, read_attr_modifytime_v04},		"modifytime"},
		    {SSH_FILEXFER_ATTR_ACL,			6,				{read_attr_zero, read_attr_acl_v04},			"acl"},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03},		"extensions"}};

static struct _valid_attrcb_s valid_ntime04[] = {
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_accesstime_n_v04},		"accesstime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_createtime_n_v04},		"createtime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_modifytime_n_v04},		"modifytime subseconds"}};


static void read_sftp_attributes(struct sftp_client_s *sftp, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct attr_version_s av04;

    memset(&av04, 0, sizeof(struct attr_version_s));
    av04.valid=valid;
    av04.attrcb=valid_attr04;
    av04.ntimecb=valid_ntime04;

    logoutput_debug("read_sftp_attributes: len %i pos %i", buffer->len , (int)(buffer->pos - buffer->buffer));
    // logoutput_base64encoded("read_sftp_attributes", buffer->buffer, buffer->len);

    /* read type (always present)
	- byte			type
    */

    av04.type=(* buffer->ops->rw.read.read_uchar)(buffer);
    attr->type=(av04.type<10) ? type_mapping[av04.type] : 0;
    attr->received|=SFTP_ATTR_TYPE;

    read_sftp_attributes_generic(sftp, &av04, 8, buffer, attr);

    logoutput_debug("read_sftp_attributes: received %i", attr->received);

}

void read_attributes_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    logoutput_debug("read_attributes_v04: valid %i", valid);
    read_sftp_attributes(sftp, valid, buffer, attr);
}

void read_sftp_features_v04(struct sftp_client_s *sftp)
{
    sftp->supported.attr_supported=0;
}

unsigned int get_attribute_mask_v04(struct sftp_client_s *sftp)
{
    return SSH_FILEXFER_STAT_VALUE;
}

int get_attribute_info_v04(struct sftp_client_s *sftp, unsigned int valid, const char *what)
{
    if (strcmp(what, "size")==0) return (valid & SSH_FILEXFER_ATTR_SIZE);
    if (strcmp(what, "uid")==0) return -1;
    if (strcmp(what, "gid")==0) return -1;
    if (strcmp(what, "user@")==0) return (valid & SSH_FILEXFER_ATTR_OWNERGROUP);
    if (strcmp(what, "group@")==0) return (valid & SSH_FILEXFER_ATTR_OWNERGROUP);
    if (strcmp(what, "perm")==0) return (valid & SSH_FILEXFER_ATTR_PERMISSIONS);
    if (strcmp(what, "acl")==0) return (valid & SSH_FILEXFER_ATTR_ACL);
    if (strcmp(what, "btime")==0) return (valid & SSH_FILEXFER_ATTR_CREATETIME);
    if (strcmp(what, "atime")==0) return (valid & SSH_FILEXFER_ATTR_ACCESSTIME);
    if (strcmp(what, "ctime")==0) return -1;
    if (strcmp(what, "mtime")==0) return (valid & SSH_FILEXFER_ATTR_MODIFYTIME);
    if (strcmp(what, "subseconds")==0) return (valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES);
    return -2;
}

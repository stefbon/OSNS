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
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "read-attr-v05.h"
#include "read-attr-v06.h"
#include "datatypes/ssh-uint.h"

static unsigned int type_mapping[10]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0, S_IFSOCK, S_IFCHR, S_IFBLK, S_IFIFO};

void read_attr_alloc_size_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    av06->version.v46.alloc_size=(* buffer->ops->rw.read.read_uint64)(buffer);
    av06->valid -= SSH_FILEXFER_ATTR_ALLOCATION_SIZE;
}

void read_attr_modifytime_n_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    av06->version.v46.modifytime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->mtime_n=av06->version.v46.modifytime_n;
}

void read_attr_changetime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    struct _valid_attrcb_s *ntimecb=av06->ntimecb;
    unsigned char flag=0;

    av06->version.v46.changetime=(* buffer->ops->rw.read.read_uint64)(buffer);
    attr->ctime=av06->version.v46.changetime;
    attr->received|=SFTP_ATTR_CTIME;
    av06->valid -= SSH_FILEXFER_ATTR_CTIME;

    flag=(av06->valid & ntimecb[4].code) >> ntimecb[4].shift;
    (* ntimecb[4].cb[flag])(sftp, buffer, av06, attr);

}

void read_attr_changetime_n_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    struct _valid_attrcb_s *ntimecb=av06->ntimecb;

    av06->version.v46.changetime_n=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->ctime_n=av06->version.v46.changetime_n;

    /* NOTE: this is the latest subseconds call for this protocol version
	so it's safe to remove, not too soon cause the SUBSECONDS
	flag is used more than once */

    av06->valid -= SSH_FILEXFER_ATTR_SUBSECOND_TIMES;
}

struct _attr_cb_s {
    struct sftp_client_s 		*sftp;
    struct attr_version_s 		*av;
    struct sftp_attr_s 			*attr;
};

static void _attr_cb_acl(struct attr_buffer_s *buffer, struct ssh_string_s *aclblock, void *ptr)
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

void read_attr_acl_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.sftp=sftp, .av=av06, .attr=attr};
    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &av06->version.v46.acl, _attr_cb_acl, (void *) &_cb_data);
    av06->valid -= SSH_FILEXFER_ATTR_ACL;
}

void read_attr_bits_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    av06->version.v46.bits=(* buffer->ops->rw.read.read_uint32)(buffer);
    av06->version.v46.bits_valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    av06->valid -= SSH_FILEXFER_ATTR_BITS;
}

void read_attr_texthint_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    av06->version.v46.texthint=(* buffer->ops->rw.read.read_uchar)(buffer);
    av06->valid -= SSH_FILEXFER_ATTR_TEXT_HINT;
}

static void _attr_cb_mimetype(struct attr_buffer_s *buffer, struct ssh_string_s *mime, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;

    /* what here? lookup the mimetype in the local mime db, and get a code, and store that value somewhere in inode->stat/st/attr */
}

void read_attr_mimetype_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.sftp=sftp, .av=av06, .attr=attr};
    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &av06->version.v46.mimetype, _attr_cb_mimetype, (void *) &_cb_data);
    av06->valid -= SSH_FILEXFER_ATTR_MIME_TYPE;
}

void read_attr_link_count_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    av06->version.v46.link_count=(* buffer->ops->rw.read.read_uint32)(buffer);
    av06->valid -= SSH_FILEXFER_ATTR_LINK_COUNT;
}

static void _attr_cb_untrans_name(struct attr_buffer_s *buffer, struct ssh_string_s *name, void *ptr)
{
    struct _attr_cb_s *data=(struct _attr_cb_s *) ptr;

    /* what here? lookup the mimetype in the local mime db, and get a code, and store that value somewhere in inode->stat/st/attr */
}

void read_attr_untranslated_name_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av06, struct sftp_attr_s *attr)
{
    struct _attr_cb_s _cb_data={.sftp=sftp, .av=av06, .attr=attr};
    uint32_t len=(* buffer->ops->rw.read.read_string)(buffer, &av06->version.v46.mimetype, _attr_cb_untrans_name, (void *) &_cb_data);
    av06->valid -= SSH_FILEXFER_ATTR_UNTRANSLATED_NAME;
}

static struct _valid_attrcb_s valid_attr06[] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}, 			"size"},
		    {SSH_FILEXFER_ATTR_ALLOCATION_SIZE, 	10,				{read_attr_zero, read_attr_alloc_size_v06},		"allocation size"},
		    {SSH_FILEXFER_ATTR_OWNERGROUP,		7,				{read_attr_zero, read_attr_ownergroup_v04},		"ownergroup"},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03},		"permissions"},
		    {SSH_FILEXFER_ATTR_ACCESSTIME, 		3,				{read_attr_zero, read_attr_accesstime_v04},		"accesstime"},
		    {SSH_FILEXFER_ATTR_CREATETIME, 		4,				{read_attr_zero, read_attr_createtime_v04},		"createtime"},
		    {SSH_FILEXFER_ATTR_MODIFYTIME,		5,				{read_attr_zero, read_attr_modifytime_v04},		"modifytime"},
		    {SSH_FILEXFER_ATTR_CTIME,			15,				{read_attr_zero, read_attr_changetime_v06},		"changetime"},
		    {SSH_FILEXFER_ATTR_ACL,			6,				{read_attr_zero, read_attr_acl_v06},			"acl"},
		    {SSH_FILEXFER_ATTR_BITS,			9,				{read_attr_zero, read_attr_bits_v06},			"bits"},
		    {SSH_FILEXFER_ATTR_TEXT_HINT,		11,				{read_attr_zero, read_attr_texthint_v06},		"text hint"},
		    {SSH_FILEXFER_ATTR_MIME_TYPE,		12,				{read_attr_zero, read_attr_mimetype_v06}, 		"mime type"},
		    {SSH_FILEXFER_ATTR_LINK_COUNT,		13,				{read_attr_zero, read_attr_link_count_v06},		"link count"},
		    {SSH_FILEXFER_ATTR_UNTRANSLATED_NAME,	14,				{read_attr_zero, read_attr_untranslated_name_v06},	"untranslated name"},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03},		"extensions"}};

static struct _valid_attrcb_s valid_ntime06[] = {
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_accesstime_n_v04},		"accesstime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_createtime_n_v04},		"createtime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_modifytime_n_v06},		"modifytime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_changetime_n_v06},		"changetime subseconds"}};



static void read_sftp_attributes(struct sftp_client_s *sftp, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct attr_version_s av06;
    unsigned char type=0;

    memset(&av06, 0, sizeof(struct attr_version_s));
    av06.valid=valid;
    av06.attrcb=valid_attr06;
    av06.ntimecb=valid_ntime06;

    logoutput_debug("read_sftp_attributes: len %i pos %i", buffer->len , (int)(buffer->pos - buffer->buffer));
    // logoutput_base64encoded("read_sftp_attributes", buffer->buffer, buffer->len);

    /* read type (always present)
	- byte			type
    */

    av06.type=(* buffer->ops->rw.read.read_uchar)(buffer);
    attr->type=(av06.type<10) ? type_mapping[av06.type] : 0;
    attr->received|=SFTP_ATTR_TYPE;

    read_sftp_attributes_generic(sftp, &av06, 15, buffer, attr);

    logoutput_debug("read_sftp_attributes: received %i", attr->received);

}

void read_attributes_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    logoutput_debug("read_attributes_v06: valid %i pos %i", valid, (int)(buffer->pos - buffer->buffer));
    read_sftp_attributes(sftp, valid, buffer, attr);
}

/* translate the mask of supported sftp attributes to a fuse mask */

void read_sftp_features_v06(struct sftp_client_s *sftp)
{
    struct sftp_supported_s *supported=&sftp->supported;
    unsigned int mask=supported->version.v06.attribute_mask;

    if (mask==0) return;

    logoutput_debug("read_sftp_features_v06: supported attribute mask %i", mask);

    supported->attr_supported=SFTP_ATTR_TYPE;
    supported->version.v06.init=1;

    if (mask & SSH_FILEXFER_ATTR_SIZE) {

	logoutput_debug("read_sftp_features_v06: sftp attr size supported");
	supported->attr_supported|=SFTP_ATTR_SIZE;

    } else {

	logoutput_debug("read_sftp_features_v06: sftp attr size not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_PERMISSIONS) {

	supported->attr_supported|=SFTP_ATTR_PERMISSIONS;
	logoutput_debug("read_sftp_features_v06: sftp attr permissions supported");

    } else {

	logoutput_debug("read_sftp_features_v06: sftp attr permissions not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_OWNERGROUP) {

	supported->attr_supported|=SFTP_ATTR_USER | SFTP_ATTR_GROUP;
	logoutput_debug("read_sftp_features_v06: sftp attr ownergroup supported");

    } else {

	logoutput_debug("read_sftp_features_v06: sftp attr ownergroup not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_ACCESSTIME) {

	supported->attr_supported|=SFTP_ATTR_ATIME;
	logoutput_debug("read_sftp_features_v06: sftp attr atime supported");

    } else {

	logoutput_debug("read_sftp_features_v06: sftp attr mtime not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_MODIFYTIME) {

	supported->attr_supported|=SFTP_ATTR_MTIME;
	logoutput_debug("read_sftp_features_v06: sftp attr mtime supported");

    } else {

	logoutput_debug("read_sftp_features_v06: sftp attr mtime not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_CTIME) {

	supported->attr_supported|=SFTP_ATTR_CTIME;
	logoutput_debug("read_sftp_features_v06: sftp attr ctime supported");

    } else {

	logoutput_debug("read_sftp_features_v06: sftp attr ctime not supported");

    }

}

unsigned int get_attribute_mask_v06(struct sftp_client_s *sftp)
{
    return (sftp->supported.version.v06.attribute_mask);
}

int get_attribute_info_v06(struct sftp_client_s *sftp, unsigned int valid, const char *what)
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
    if (strcmp(what, "ctime")==0) return (valid & SSH_FILEXFER_ATTR_CTIME);
    if (strcmp(what, "mtime")==0) return (valid & SSH_FILEXFER_ATTR_MODIFYTIME);
    if (strcmp(what, "subseconds")==0) return (valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES);
    return -2;
}

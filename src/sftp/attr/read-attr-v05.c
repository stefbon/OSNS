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

#include "logging.h"
#include "main.h"
#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v05.h"
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "read-attr-v05.h"
#include "datatypes/ssh-uint.h"

/* more information:

   https://tools.ietf.org/html/draft-ietf-secsh-filexfer-05#section-5 */

static unsigned int type_mapping[10]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0, S_IFSOCK, S_IFCHR, S_IFBLK, S_IFIFO};
static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_FIFO, SSH_FILEXFER_TYPE_CHAR_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_BLOCK_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SOCKET};

void read_attr_bits_v05(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av05, struct sftp_attr_s *attr)
{
    av05->version.v46.bits=(* buffer->ops->rw.read.read_uint32)(buffer);
    av05->valid -= SSH_FILEXFER_ATTR_BITS;
}

static struct _valid_attrcb_s valid_attr05[] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}, 			"size"},
		    {SSH_FILEXFER_ATTR_OWNERGROUP,		7,				{read_attr_zero, read_attr_ownergroup_v04},		"ownergroup"},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03},		"permissions"},
		    {SSH_FILEXFER_ATTR_ACCESSTIME, 		3,				{read_attr_zero, read_attr_accesstime_v04},		"accesstime"},
		    {SSH_FILEXFER_ATTR_CREATETIME, 		4,				{read_attr_zero, read_attr_createtime_v04},		"createtime"},
		    {SSH_FILEXFER_ATTR_MODIFYTIME,		5,				{read_attr_zero, read_attr_modifytime_v04},		"modifytime"},
		    {SSH_FILEXFER_ATTR_ACL,			6,				{read_attr_zero, read_attr_acl_v04},			"acl"},
		    {SSH_FILEXFER_ATTR_BITS,			9,				{read_attr_zero, read_attr_bits_v05},			"bits"},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03},		"extensions"}};

static struct _valid_attrcb_s valid_ntime05[] = {
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_createtime_n_v04},		"createtime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_modifytime_n_v04},		"modifytime subseconds"},
		    {SSH_FILEXFER_ATTR_SUBSECOND_TIMES,		8,				{read_attr_zero, read_attr_accesstime_n_v04},		"accesstime subseconds"}};


static void read_sftp_attributes(struct sftp_client_s *sftp, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct attr_version_s av05;
    unsigned char type=0;

    memset(&av05, 0, sizeof(struct attr_version_s));
    av05.valid=valid;
    av05.attrcb=valid_attr05;
    av05.ntimecb=valid_ntime05;

    logoutput_debug("read_sftp_attributes: len %i pos %i", buffer->len , (int)(buffer->pos - buffer->buffer));
    logoutput_base64encoded("read_sftp_attributes", buffer->buffer, buffer->len);

    /* read type (always present)
	- byte			type
    */

    av05.type=(* buffer->ops->rw.read.read_uchar)(buffer);
    attr->type=(av05.type<10) ? type_mapping[av05.type] : 0;
    attr->received|=SFTP_ATTR_TYPE;

    read_sftp_attributes_generic(sftp, &av05, 9, buffer, attr);
    logoutput_debug("read_sftp_attributes: received %i", attr->received);

}

void read_attributes_v05(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    logoutput_debug("read_attributes_v05: valid %i", valid);
    read_sftp_attributes(sftp, valid, buffer, attr);
}

void read_sftp_features_v05(struct sftp_client_s *sftp)
{
    struct sftp_supported_s *supported=&sftp->supported;
    unsigned int mask=supported->version.v05.attribute_mask;

    if (mask==0) return;

    supported->attr_supported=SFTP_ATTR_TYPE;
    supported->version.v05.init=1;

    if (mask & SSH_FILEXFER_ATTR_SIZE) {

	logoutput_debug("read_sftp_features_v05: sftp attr size supported");
	supported->attr_supported|=SFTP_ATTR_SIZE;

    } else {

	logoutput_debug("read_sftp_features_v05: sftp attr size not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_PERMISSIONS) {

	supported->attr_supported|=SFTP_ATTR_PERMISSIONS;
	logoutput_debug("read_sftp_features_v05: sftp attr permissions supported");

    } else {

	logoutput_debug("read_sftp_features_v05: sftp attr permissions not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_OWNERGROUP) {

	supported->attr_supported|=SFTP_ATTR_USER | SFTP_ATTR_GROUP;
	logoutput_debug("read_sftp_features_v05: sftp attr ownergroup supported");

    } else {

	logoutput_debug("read_sftp_features_v05: sftp attr ownergroup not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_ACCESSTIME) {

	supported->attr_supported|=SFTP_ATTR_ATIME;
	logoutput_debug("read_sftp_features_v05: sftp attr atime supported");

    } else {

	logoutput_debug("read_sftp_features_v05: sftp attr mtime not supported");

    }

    if (mask & SSH_FILEXFER_ATTR_MODIFYTIME) {

	supported->attr_supported|=SFTP_ATTR_MTIME;
	logoutput_debug("read_sftp_features_v05: sftp attr mtime supported");

    } else {

	logoutput_debug("read_sftp_features_v05: sftp attr mtime not supported");

    }

}

unsigned int get_attribute_mask_v05(struct sftp_client_s *sftp)
{
    return (sftp->supported.version.v05.attribute_mask);
}

int get_attribute_info_v05(struct sftp_client_s *sftp, unsigned int valid, const char *what)
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



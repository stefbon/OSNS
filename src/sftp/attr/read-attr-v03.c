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
#include "read-attr-v03.h"

static unsigned int type_mapping[5]={0, S_IFREG, S_IFDIR, S_IFLNK, 0};

void read_attr_zero(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr)
{
}

void read_attr_size_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr)
{
    av03->size=(* buffer->ops->rw.read.read_uint64)(buffer);
    attr->size=av03->size;
    attr->received|=SFTP_ATTR_SIZE;
    av03->valid -= SSH_FILEXFER_ATTR_SIZE;
}

void read_attr_uidgid_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr)
{
    struct sftp_usermapping_s *usermapping=&sftp->usermapping;
    struct sftp_user_s user;
    struct sftp_group_s group;

    /* uid */

    av03->version.v03.uid=(* buffer->ops->rw.read.read_uint32)(buffer);

    user.remote.id=av03->version.v03.uid;
    (* usermapping->get_local_uid)(sftp, &user);
    attr->user.uid=user.uid;

    attr->received|=SFTP_ATTR_USER;

    /* gid */

    av03->version.v03.gid=(* buffer->ops->rw.read.read_uint32)(buffer);

    group.remote.id=av03->version.v03.gid;
    (* usermapping->get_local_gid)(sftp, &group);
    attr->group.gid=group.gid;

    attr->received|=SFTP_ATTR_GROUP;
    av03->valid -= SSH_FILEXFER_ATTR_UIDGID;

    logoutput("read_attr_uidgid_v03: remote %i:%i local %i:%i", av03->version.v03.uid, av03->version.v03.gid, attr->user.uid, attr->group.gid);
}

void read_attr_permissions_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr)
{
    av03->permissions=(* buffer->ops->rw.read.read_uint32)(buffer);

    attr->permissions=(S_IRWXU | S_IRWXG | S_IRWXO) & av03->permissions; /* sftp uses the same permission bits as Linux */
    attr->received|=SFTP_ATTR_PERMISSIONS;
    av03->valid -= SSH_FILEXFER_ATTR_PERMISSIONS;

}

void read_attr_acmodtime_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr)
{
    /* access time */

    av03->version.v03.accesstime=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->atime=av03->version.v03.accesstime;
    attr->received|=SFTP_ATTR_ATIME;

    /* modify time */

    av03->version.v03.modifytime=(* buffer->ops->rw.read.read_uint32)(buffer);
    attr->mtime=av03->version.v03.modifytime;
    attr->received|=SFTP_ATTR_MTIME;
    av03->valid -= SSH_FILEXFER_ATTR_ACMODTIME;

}

void read_attr_extensions_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr)
{
    /* what to do here ? */
    av03->valid -= SSH_FILEXFER_ATTR_EXTENDED;
}

static struct _valid_attrcb_s  valid_attr03[5] = {
		    {SSH_FILEXFER_ATTR_SIZE, 			0,				{read_attr_zero, read_attr_size_v03}, 			"size"},
		    {SSH_FILEXFER_ATTR_UIDGID,			1,				{read_attr_zero, read_attr_uidgid_v03},			"uidgid"},
		    {SSH_FILEXFER_ATTR_PERMISSIONS, 		2,				{read_attr_zero, read_attr_permissions_v03},		"permissions"},
		    {SSH_FILEXFER_ATTR_ACMODTIME, 		3,				{read_attr_zero, read_attr_acmodtime_v03},		"acmodtime"},
		    {SSH_FILEXFER_ATTR_EXTENDED,		31,				{read_attr_zero, read_attr_extensions_v03},		"extensions"}};

void read_sftp_attributes_generic(struct sftp_client_s *sftp, struct attr_version_s *av, unsigned int count, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned char ctr=0;
    uint32_t flag=0;

    while (ctr<count && av->valid>0) {

	/* following gives a 0 or 1 if flag is set */

	flag=(av->valid & av->attrcb[ctr].code) >> av->attrcb[ctr].shift;

	logoutput_debug("read_sftp_attributes_generic: %s valid %i ctr %i flag %i pos %i", av->attrcb[ctr].name, av->valid, ctr, flag, (int)(buffer->pos - buffer->buffer));

	/* run the cb if flag 1 or the "do nothing" cb if flag 0*/

	(* av->attrcb[ctr].cb[flag])(sftp, buffer, av, attr);
	ctr++;

    }

    // logoutput_debug("read_sftp_attributes_generic: received %i", attr->received);

}


void read_sftp_attributes_v03(struct sftp_client_s *sftp, unsigned int valid, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct attr_version_s av03;

    memset(&av03, 0, sizeof(struct attr_version_s));
    av03.valid=valid;
    av03.attrcb=valid_attr03;

    logoutput_debug("read_sftp_attributes: len %i pos %i", buffer->len , (int)(buffer->pos - buffer->buffer));
    // logoutput_base64encoded("read_sftp_attributes", buffer->buffer, buffer->len);

    read_sftp_attributes_generic(sftp, &av03, 5, buffer, attr);

    logoutput_debug("read_sftp_attributes: received %i", attr->received);

}

void read_attributes_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr)
{
    unsigned int valid=(* buffer->ops->rw.read.read_uint32)(buffer);
    logoutput_debug("read_attributes_v03: valid %i", valid);
    read_sftp_attributes_v03(sftp, valid, buffer, attr);
}

void read_sftp_features_v03(struct sftp_client_s *sftp)
{
    struct sftp_supported_s *supported=&sftp->supported;
    supported->attr_supported = 0;
}

unsigned int get_attribute_mask_v03(struct sftp_client_s *sftp)
{
    return SSH_FILEXFER_STAT_VALUE;
}

int get_attribute_info_v03(struct sftp_client_s *sftp, unsigned int valid, const char *what)
{
    if (strcmp(what, "size")==0) return (valid && SSH_FILEXFER_ATTR_SIZE);
    if (strcmp(what, "uid")==0) return (valid && SSH_FILEXFER_ATTR_UIDGID);
    if (strcmp(what, "gid")==0) return (valid && SSH_FILEXFER_ATTR_UIDGID);
    if (strcmp(what, "user@")==0) return -1;
    if (strcmp(what, "group@")==0) return -1;
    if (strcmp(what, "perm")==0) return (valid && SSH_FILEXFER_ATTR_PERMISSIONS);
    if (strcmp(what, "acl")==0) return -1;
    if (strcmp(what, "btime")==0) return -1;
    if (strcmp(what, "ctime")==0) return -1;
    if (strcmp(what, "atime")==0 || strcmp(what, "mtime")==0) return (valid && SSH_FILEXFER_ATTR_ACMODTIME);
    if (strcmp(what, "subseconds")==0) return -1;
    return -2;
}

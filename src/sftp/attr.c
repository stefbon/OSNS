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

#include "main.h"
#include "log.h"
#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/protocol-v06.h"
#include "sftp/attr-context.h"
#include "common.h"
#include "init.h"

#include "sftp/attr-read.h"
#include "sftp/attr-write.h"

void set_sftp_attr_version(struct sftp_client_s *sftp)
{
    unsigned char version=get_sftp_protocol_version(sftp);
    struct sftp_attr_ops_s *ops=&sftp->attr_ops;

    if (version<=3) {

	ops->read_attributes			= read_attributes_v03;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attr_nameresponse_v03;
	ops->write_attributes			= write_attributes_v03;
	ops->write_attributes_len		= write_attributes_len_v03;

    } else if (version==4) {

	ops->read_attributes			= read_attributes_v04;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attributes_v04;
	ops->write_attributes			= write_attributes_v04;
	ops->write_attributes_len		= write_attributes_len_v04;

    } else if (version==5) {

	ops->read_attributes			= read_attributes_v05;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attributes_v05;
	ops->write_attributes			= write_attributes_v05;
	ops->write_attributes_len		= write_attributes_len_v05;

    } else if (version==6) {

	ops->read_attributes			= read_attributes_v06;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attributes_v06;
	ops->write_attributes			= write_attributes_v06;
	ops->write_attributes_len		= write_attributes_len_v06;

    } else {

	logoutput_warning("set_sftp_attr_version: error sftp protocol version %i not supported", version);

    }

}

static unsigned int get_sftp_client_flags(struct attr_context_s *ctx, const char *what)
{

    if (strcmp(what, "newreaddir")==0) {
	struct sftp_client_s *sftp=(struct sftp_client_s *)((char *) ctx - offsetof(struct sftp_client_s, attrctx));

	return (sftp->flags && SFTP_CLIENT_FLAG_NEWREADDIR);

    }

    return 0;

}

static unsigned char get_sftp_client_protocol_version(struct attr_context_s *ctx)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)((char *) ctx - offsetof(struct sftp_client_s, attrctx));

    return get_sftp_protocol_version(sftp);
}

void init_sftp_client_attr_context(struct sftp_client_s *sftp)
{

    init_attr_context(&sftp->attrctx, NULL);

    sftp->attrctx.get_sftp_flags=get_sftp_client_flags;
    sftp->attrctx.get_sftp_protocol_version=get_sftp_client_protocol_version;
}

void read_sftp_features(struct sftp_client_s *sftp)
{
    struct sftp_supported_s *supported=&sftp->supported;
    unsigned char version=get_sftp_protocol_version(sftp);
    unsigned int mask=0;

    /* what to with versions 3,4 ? */

    if (version==5) {

	mask=supported->version.v05.attribute_mask;
	supported->version.v05.init=1;

    } else if (version==6) {

	mask=supported->version.v06.attribute_mask;
	supported->version.v06.init=1;

    }

    supported->attr_supported=SFTP_ATTR_TYPE;

    if (mask>0) {
	struct sftp_supported_s *supported=&sftp->supported;

	if (mask & SSH_FILEXFER_ATTR_SIZE) {

	    supported->attr_supported|=SFTP_ATTR_SIZE;

	} else {

	    logoutput_debug("read_sftp_features: sftp attr size not supported");

	}

	if (mask & SSH_FILEXFER_ATTR_PERMISSIONS) {

	    supported->attr_supported|=SFTP_ATTR_PERMISSIONS;

	} else {

	    logoutput_debug("read_sftp_features: sftp attr permissions not supported");

	}

	if (mask & SSH_FILEXFER_ATTR_OWNERGROUP) {

	    supported->attr_supported|=SFTP_ATTR_USER | SFTP_ATTR_GROUP;

	} else {

	    logoutput_debug("read_sftp_features: sftp attr ownergroup not supported");

	}

	if (mask & SSH_FILEXFER_ATTR_ACCESSTIME) {

	    supported->attr_supported|=SFTP_ATTR_ATIME;

	} else {

	    logoutput_debug("read_sftp_features: sftp attr mtime not supported");

	}

	if (mask & SSH_FILEXFER_ATTR_MODIFYTIME) {

	    supported->attr_supported|=SFTP_ATTR_MTIME;

	} else {

	    logoutput_debug("read_sftp_features: sftp attr mtime not supported");

	}

	if (mask & SSH_FILEXFER_ATTR_CTIME) {

	    supported->attr_supported|=SFTP_ATTR_CTIME;

	} else {

	    logoutput_debug("read_sftp_features: sftp attr ctime not supported");

	}

    }

}

unsigned int get_sftp_attribute_mask(struct sftp_client_s *sftp)
{
    unsigned char version=get_sftp_protocol_version(sftp);
    unsigned int mask=0;

    /* what to with versions 3,4 ? */

    if (version==3 || version==4) {

	mask=SSH_FILEXFER_STAT_VALUE;

    } else if (version==5) {

	mask=sftp->supported.version.v05.attribute_mask;

    } else if (version==6) {

	mask=sftp->supported.version.v06.attribute_mask;

    }

    return mask;
}

int get_sftp_attribute_info(unsigned char version, unsigned int valid, const char *what)
{

    if (version==3) {

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

    } else if (version==4) {

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

    } else if (version==5) {

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

    } else if (version==6) {

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

    }

    return -2;
}

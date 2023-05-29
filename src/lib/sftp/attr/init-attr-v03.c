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

#include "libosns-basic-system-headers.h"

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-error.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v03.h"
#include "sftp/attr-context.h"

#include "write-attr-v03.h"
#include "read-attr-v03.h"
#include "init-attr-v03.h"

void init_attr_context_v03(struct attr_context_s *actx)
{
    struct _rw_attrcb_s *attrcb=actx->attrcb;

    init_attrcb_zero(attrcb, ATTR_CONTEXT_COUNT_ATTR_CB);

    /* NOTE: with version 3 no type in ATTR */

    /* size */

    attrcb[SSH_FILEXFER_INDEX_SIZE].code		= SSH_FILEXFER_ATTR_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].shift		= SSH_FILEXFER_INDEX_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].w_cb		= write_attr_size_v03;
    attrcb[SSH_FILEXFER_INDEX_SIZE].r_cb		= read_attr_size_v03;
    attrcb[SSH_FILEXFER_INDEX_SIZE].maxlength		= 8;
    attrcb[SSH_FILEXFER_INDEX_SIZE].name		= "size";
    attrcb[SSH_FILEXFER_INDEX_SIZE].stat_mask		= SYSTEM_STAT_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].fattr		= FATTR_SIZE;

    /* owner and group == uid and gid with version 3 */

    attrcb[SSH_FILEXFER_INDEX_UIDGID].code		= SSH_FILEXFER_ATTR_UIDGID;
    attrcb[SSH_FILEXFER_INDEX_UIDGID].shift		= SSH_FILEXFER_INDEX_UIDGID;
    attrcb[SSH_FILEXFER_INDEX_UIDGID].w_cb		= write_attr_uidgid_v03;
    attrcb[SSH_FILEXFER_INDEX_UIDGID].r_cb		= read_attr_uidgid_v03;
    attrcb[SSH_FILEXFER_INDEX_UIDGID].maxlength		= 8;
    attrcb[SSH_FILEXFER_INDEX_UIDGID].name		= "uidgid";
    attrcb[SSH_FILEXFER_INDEX_UIDGID].stat_mask		= SYSTEM_STAT_UID | SYSTEM_STAT_GID;
    attrcb[SSH_FILEXFER_INDEX_UIDGID].fattr		= FATTR_UID | FATTR_GID;

    /* permissions = posix mode */

    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].code		= SSH_FILEXFER_ATTR_PERMISSIONS;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].shift	= SSH_FILEXFER_INDEX_PERMISSIONS;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].w_cb		= write_attr_permissions_v03;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].r_cb		= read_attr_permissions_v03;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].maxlength	= 4;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].name		= "permissions and type";
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].stat_mask	= SYSTEM_STAT_MODE | SYSTEM_STAT_TYPE;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].fattr	= FATTR_MODE;

    /* access and modify time in one */

    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].code		= SSH_FILEXFER_ATTR_ACMODTIME;
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].shift		= SSH_FILEXFER_INDEX_ACMODTIME;
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].w_cb		= write_attr_acmodtime_v03;
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].r_cb		= read_attr_acmodtime_v03;
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].maxlength	= 8;
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].name		= "acmodtime";
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].stat_mask	= SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME;
    attrcb[SSH_FILEXFER_INDEX_ACMODTIME].fattr		= FATTR_ATIME | FATTR_MTIME;

    /* extensions */

    attrcb[SSH_FILEXFER_INDEX_EXTENDED].code		= SSH_FILEXFER_ATTR_EXTENDED;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].shift		= SSH_FILEXFER_INDEX_EXTENDED;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].r_cb		= read_attr_extensions_v03;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].maxlength	= 0;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].name		= "extended";
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].stat_mask	= 0;

    actx->w_valid.mask=(SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME);
    actx->w_valid.flags=0;
    actx->w_count=4;
    actx->r_valid.mask=(SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME | SSH_FILEXFER_ATTR_EXTENDED);
    actx->r_valid.flags=0;
    actx->r_count=5;

}

/*
	prepare the reading or writing of attributes following the rules in

	https://datatracker.ietf.org/doc/html/draft-ietf-secsh-filexfer-02#section-5

        uint32   flags
        uint64   size           present only if flag SSH_FILEXFER_ATTR_SIZE
        uint32   uid            present only if flag SSH_FILEXFER_ATTR_UIDGID
        uint32   gid            present only if flag SSH_FILEXFER_ATTR_UIDGID
        uint32   permissions    present only if flag SSH_FILEXFER_ATTR_PERMISSIONS
        uint32   atime          present only if flag SSH_FILEXFER_ACMODTIME
        uint32   mtime          present only if flag SSH_FILEXFER_ACMODTIME
        uint32   extended_count present only if flag SSH_FILEXFER_ATTR_EXTENDED
        string   extended_type
        string   extended_data
        ...      more extended data (extended_type - extended_data pairs),
                   so that number of pairs equals extended_count

	NOTE:
	    - specific attributes are only present if bit in the first 4 bytes ("flags") is set for that property
	    - the order is important

*/

void parse_attributes_v03(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{

    /* NOTE: no type, is part of the permissions field */

    if (r->todo & SSH_FILEXFER_ATTR_SIZE) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_SIZE);
    if (r->todo & SSH_FILEXFER_ATTR_UIDGID) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_UIDGID);
    if (r->todo & SSH_FILEXFER_ATTR_PERMISSIONS) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_PERMISSIONS);
    if (r->todo & SSH_FILEXFER_ATTR_ACMODTIME) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_ACMODTIME);
    if (r->todo & SSH_FILEXFER_ATTR_EXTENDED) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_EXTENDED);

}

unsigned char enable_attr_v03(struct attr_context_s *actx, struct sftp_valid_s *p, const char *name)
{
    unsigned char result=0;

    if (strcmp(name, "size")==0) {

	p->mask |= SSH_FILEXFER_ATTR_SIZE;
	result=1;

    } else if (strcmp(name, "permissions")==0) {

	p->mask |= SSH_FILEXFER_ATTR_PERMISSIONS;
	result=1;

    } else if (strcmp(name, "user")==0 || strcmp(name, "group")==0) {

	p->mask |= SSH_FILEXFER_ATTR_UIDGID;
	result=1;

    } else if (strcmp(name, "atime")==0 || strcmp(name, "mtime")==0) {

	p->mask |= SSH_FILEXFER_ATTR_ACMODTIME;
	result=1;

    } else if (strcmp(name, "extended")==0) {

	p->mask |= SSH_FILEXFER_ATTR_EXTENDED;
	result=1;

    }

    return result;
}

unsigned int get_property_v03(struct attr_context_s *actx, unsigned int flag)
{
    unsigned int result=0;

    if (flag & SFTP_ATTR_PROPERTY_VALIDFIELD_STAT) {

	result |= 0;

    }

    return result;
}

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

#include "libosns-basic-system-headers.h"

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-error.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v05.h"

#include "sftp/attr-context.h"

#include "write-attr-v03.h"
#include "write-attr-v04.h"
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "init-attr-v04.h"

void init_attr_context_v05(struct attr_context_s *actx)
{
    struct _rw_attrcb_s *attrcb=actx->attrcb;

    init_attrcb_zero(attrcb, ATTR_CONTEXT_COUNT_ATTR_CB);

    /* type */

    attrcb[SSH_FILEXFER_INDEX_TYPE].code		= SSH_FILEXFER_ATTR_TYPE;
    attrcb[SSH_FILEXFER_INDEX_TYPE].shift		= SSH_FILEXFER_INDEX_TYPE;
    attrcb[SSH_FILEXFER_INDEX_TYPE].w_cb		= write_attr_type_v05;
    attrcb[SSH_FILEXFER_INDEX_TYPE].r_cb		= read_attr_type_v05;
    attrcb[SSH_FILEXFER_INDEX_TYPE].maxlength		= 1;
    attrcb[SSH_FILEXFER_INDEX_TYPE].name		= "type";
    attrcb[SSH_FILEXFER_INDEX_TYPE].stat_mask		= SYSTEM_STAT_TYPE;
    attrcb[SSH_FILEXFER_INDEX_TYPE].fattr		= 0;

    /* size */

    attrcb[SSH_FILEXFER_INDEX_SIZE].code		= SSH_FILEXFER_ATTR_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].shift		= SSH_FILEXFER_INDEX_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].w_cb		= write_attr_size_v03;
    attrcb[SSH_FILEXFER_INDEX_SIZE].r_cb		= read_attr_size_v03;
    attrcb[SSH_FILEXFER_INDEX_SIZE].maxlength		= 8; /* size uses 8 bytes (=64 bits) */
    attrcb[SSH_FILEXFER_INDEX_SIZE].name		= "size";
    attrcb[SSH_FILEXFER_INDEX_SIZE].stat_mask		= SYSTEM_STAT_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].fattr		= FATTR_SIZE;

    /* owner and group */

    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].code		= SSH_FILEXFER_ATTR_OWNERGROUP;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].shift		= SSH_FILEXFER_INDEX_OWNERGROUP;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].w_cb		= write_attr_ownergroup_v04;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].r_cb		= read_attr_ownergroup_v04;
    /* both owner and group are in name@domain format */
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].maxlength	= ((* actx->maxlength_username)(actx) + 5 + (* actx->maxlength_groupname)(actx) + 5 + 2 * ((* actx->maxlength_domainname)(actx)));
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].name		= "ownergroup";
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].stat_mask	= SYSTEM_STAT_UID | SYSTEM_STAT_GID;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].fattr		= FATTR_UID | FATTR_GID;

    /* permissions = posix mode */

    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].code		= SSH_FILEXFER_ATTR_PERMISSIONS;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].shift	= SSH_FILEXFER_INDEX_PERMISSIONS;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].w_cb		= write_attr_permissions_v04;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].r_cb		= read_attr_permissions_v04;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].maxlength	= 4; /* perm uses 4 bytes (=32 bits) */
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].name		= "permissions";
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].stat_mask	= SYSTEM_STAT_MODE;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].fattr	= FATTR_MODE;

    /* access time */

    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].code		= SSH_FILEXFER_ATTR_ACCESSTIME;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].shift		= SSH_FILEXFER_INDEX_ACCESSTIME;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].w_cb		= write_attr_accesstime_v04;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].r_cb		= read_attr_accesstime_v04;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].maxlength	= 8; /* both access and modify time use 8 bytes (=64 bits) */
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].name		= "accesstime";
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].stat_mask	= SYSTEM_STAT_ATIME;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].fattr		= FATTR_ATIME;

    /* create time */

    attrcb[SSH_FILEXFER_INDEX_CREATETIME].code		= SSH_FILEXFER_ATTR_CREATETIME;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].shift		= SSH_FILEXFER_INDEX_CREATETIME;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].r_cb		= read_attr_createtime_v04;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].maxlength	= 8; /* both access and modify time use 8 bytes (=64 bits) */
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].name		= "createtime";
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].stat_mask	= SYSTEM_STAT_BTIME;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].fattr		= 0;

    /* modify time */

    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].code		= SSH_FILEXFER_ATTR_MODIFYTIME;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].shift		= SSH_FILEXFER_INDEX_MODIFYTIME;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].w_cb		= write_attr_modifytime_v04;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].r_cb		= read_attr_modifytime_v04;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].maxlength	= 8;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].name		= "modifytime";
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].stat_mask	= SYSTEM_STAT_MTIME;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].fattr		= FATTR_MTIME;

    /* ACL */

    attrcb[SSH_FILEXFER_INDEX_ACL].code			= SSH_FILEXFER_ATTR_ACL;
    attrcb[SSH_FILEXFER_INDEX_ACL].shift		= SSH_FILEXFER_INDEX_ACL;
    attrcb[SSH_FILEXFER_INDEX_ACL].w_cb			= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_ACL].r_cb			= read_attr_acl_v04;
    attrcb[SSH_FILEXFER_INDEX_ACL].maxlength		= 0; /* unknown and not supported */
    attrcb[SSH_FILEXFER_INDEX_ACL].name			= "ACL";
    attrcb[SSH_FILEXFER_INDEX_ACL].stat_mask		= 0;
    attrcb[SSH_FILEXFER_INDEX_ACL].fattr		= 0;

    /* attrib bits */

    attrcb[SSH_FILEXFER_INDEX_BITS].code		= SSH_FILEXFER_ATTR_BITS;
    attrcb[SSH_FILEXFER_INDEX_BITS].shift		= SSH_FILEXFER_INDEX_BITS;
    attrcb[SSH_FILEXFER_INDEX_BITS].w_cb		= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_BITS].r_cb		= read_attr_bits_v05;
    attrcb[SSH_FILEXFER_INDEX_BITS].maxlength		= 4;
    attrcb[SSH_FILEXFER_INDEX_BITS].name		= "attrib bits";
    attrcb[SSH_FILEXFER_INDEX_BITS].stat_mask		= 0;
    attrcb[SSH_FILEXFER_INDEX_BITS].fattr		= 0;

    /* extensions */

    attrcb[SSH_FILEXFER_INDEX_EXTENDED].code		= SSH_FILEXFER_ATTR_EXTENDED;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].shift		= SSH_FILEXFER_INDEX_EXTENDED;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].r_cb		= read_attr_extensions_v03;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].maxlength	= 0;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].name		= "extended";
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].stat_mask	= 0;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].fattr		= 0;

    actx->w_valid.mask=(SSH_FILEXFER_ATTR_TYPE | SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_OWNERGROUP | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_MODIFYTIME);
    actx->w_valid.flags=SSH_FILEXFER_ATTR_SUBSECOND_TIMES;
    actx->w_count=8;
    actx->r_count=10;
    actx->r_valid.mask=(SSH_FILEXFER_ATTR_TYPE | SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_OWNERGROUP | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_ACL | SSH_FILEXFER_ATTR_BITS | SSH_FILEXFER_ATTR_EXTENDED);
    actx->r_valid.flags=SSH_FILEXFER_ATTR_SUBSECOND_TIMES;

    /* nsec accesstime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].w_cb		= write_attr_accesstime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].w_cb		= read_attr_accesstime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].maxlength	= 4; /* nsec time use 4 bytes (=32 bits) */
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].name		= "nsec accesstime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].stat_mask	= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].fattr		= 0;

    /* nsec createtime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].w_cb		= read_attr_createtime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].maxlength	= 4; /* nsec time use 4 bytes (=32 bits) */
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].name		= "nsec createtime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].stat_mask	= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].fattr		= 0;

    /* nsec modifytime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].w_cb		= write_attr_modifytime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].r_cb		= read_attr_modifytime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].maxlength	= 4;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].name		= "nsec modifytime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].stat_mask	= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].fattr		= 0;

}

/*
    prepare the reading or writing of attributes following the rules in

    https://datatracker.ietf.org/doc/html/draft-ietf-secsh-filexfer-05#section-5

       uint32   valid-attribute-flags
       byte     type                 always present
       uint64   size                 present only if flag SIZE
       string   owner                present only if flag OWNERGROUP
       string   group                present only if flag OWNERGROUP
       uint32   permissions          present only if flag PERMISSIONS
       int64    atime                present only if flag ACCESSTIME
       uint32   atime_nseconds       present only if flag SUBSECOND_TIMES
       int64    createtime           present only if flag CREATETIME
       uint32   createtime_nseconds  present only if flag SUBSECOND_TIMES
       int64    mtime                present only if flag MODIFYTIME
       uint32   mtime_nseconds       present only if flag SUBSECOND_TIMES
       string   acl                  present only if flag ACL
       uint32   attrib-bits          present only if flag BITS
       uint32   extended_count       present only if flag EXTENDED
       string   extended_type
       string   extended_data
       ...      more extended data (extended_type - extended_data pairs),
                so that number of pairs equals extended_count

	NOTE:
	    - specific attributes are only present if bit in the first 4 bytes ("flags") is set for that property
	    - the order is important

*/


void parse_attributes_v05(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{

    /* there is always a type */

    (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_TYPE);
    if (r->todo & SSH_FILEXFER_ATTR_SIZE) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_SIZE);
    if (r->todo & SSH_FILEXFER_ATTR_OWNERGROUP) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_OWNERGROUP);
    if (r->todo & SSH_FILEXFER_ATTR_PERMISSIONS) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_PERMISSIONS);

    if (r->todo & SSH_FILEXFER_ATTR_ACCESSTIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_ACCESSTIME);
	if (r->valid.flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_ATIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_CREATETIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_CREATETIME);
	if (r->valid.flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_BTIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_MODIFYTIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_MODIFYTIME);
	if (r->valid.flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_MTIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_ACL) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_ACL);
    if (r->todo & SSH_FILEXFER_ATTR_BITS) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_BITS);
    if (r->todo & SSH_FILEXFER_ATTR_EXTENDED) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_EXTENDED);

}

unsigned char enable_attr_v05(struct attr_context_s *actx, struct sftp_valid_s *p, const char *name)
{
    return enable_attr_v04(actx, p, name);
}

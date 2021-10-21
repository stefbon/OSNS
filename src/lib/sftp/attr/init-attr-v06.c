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
#include "rw-attr-generic.h"

#include "write-attr-v03.h"
#include "write-attr-v04.h"
#include "write-attr-v05.h"
#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "read-attr-v05.h"

void init_attr_context_v06(struct attr_context_s *actx)
{
    struct _rw_attrcb_s *attrcb=actx->attrcb;

    init_attrcb_zero(attrcb, 36);

    /* type */

    attrcb[SSH_FILEXFER_INDEX_TYPE].code		= SSH_FILEXFER_ATTR_TYPE;
    attrcb[SSH_FILEXFER_INDEX_TYPE].shift		= SSH_FILEXFER_INDEX_TYPE;
    attrcb[SSH_FILEXFER_INDEX_TYPE].w_cb		= write_attr_type_v06;
    attrcb[SSH_FILEXFER_INDEX_TYPE].r_cb		= read_attr_type_v06;
    attrcb[SSH_FILEXFER_INDEX_TYPE].maxlength		= 1;
    attrcb[SSH_FILEXFER_INDEX_TYPE].name		= "type";
    attrcb[SSH_FILEXFER_INDEX_TYPE].stat_mask		= SYSTEM_STAT_TYPE;

    /* size */

    attrcb[SSH_FILEXFER_INDEX_SIZE].code		= SSH_FILEXFER_ATTR_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].shift		= SSH_FILEXFER_INDEX_SIZE;
    attrcb[SSH_FILEXFER_INDEX_SIZE].w_cb		= write_attr_size_v03;
    attrcb[SSH_FILEXFER_INDEX_SIZE].r_cb		= read_attr_size_v03;
    attrcb[SSH_FILEXFER_INDEX_SIZE].maxlength		= 8; /* size uses 8 bytes (=64 bits) */
    attrcb[SSH_FILEXFER_INDEX_SIZE].name		= "size";
    attrcb[SSH_FILEXFER_INDEX_SIZE].stat_mask		= SYSTEM_STAT_SIZE;

    /* owner and group */

    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].code		= SSH_FILEXFER_ATTR_OWNERGROUP;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].shift		= SSH_FILEXFER_INDEX_OWNERGROUP;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].w_cb		= write_attr_ownergroup_v04;
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].r_cb		= read_attr_ownergroup_v04;
    /* both owner and group are in name@domain format */
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].maxlength	= ((* actx->maxlength_username)(actx) + 5 + (* actx->maxlength_groupname)(actx) + 5 + 2 * ((* actx->maxlength_domainname)(actx)));
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].name		= "ownergroup";
    attrcb[SSH_FILEXFER_INDEX_OWNERGROUP].stat_mask	= SYSTEM_STAT_UID | SYSTEM_STAT_GID;

    /* permissions = posix mode */

    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].code		= SSH_FILEXFER_ATTR_PERMISSIONS;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].shift	= SSH_FILEXFER_INDEX_PERMISSIONS;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].w_cb		= write_attr_permissions_v04;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].r_cb		= read_attr_permissions_v04;
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].maxlength	= 4; /* perm uses 4 bytes (=32 bits) */
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].name		= "permissions";
    attrcb[SSH_FILEXFER_INDEX_PERMISSIONS].stat_mask	= SYSTEM_STAT_MODE;

    /* access time */

    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].code		= SSH_FILEXFER_ATTR_ACCESSTIME;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].shift		= SSH_FILEXFER_INDEX_ACCESSTIME;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].w_cb		= write_attr_accesstime_v04;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].r_cb		= read_attr_accesstime_v04;
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].maxlength	= 8; /* both access and modify time use 8 bytes (=64 bits) */
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].name		= "accesstime";
    attrcb[SSH_FILEXFER_INDEX_ACCESSTIME].stat_mask	= SYSTEM_STAT_ATIME;

    /* create time */

    attrcb[SSH_FILEXFER_INDEX_CREATETIME].code		= SSH_FILEXFER_ATTR_CREATETIME;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].shift		= SSH_FILEXFER_INDEX_CREATETIME;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].r_cb		= read_attr_createtime_v04;
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].maxlength	= 8; /* both access and modify time use 8 bytes (=64 bits) */
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].name		= "createtime";
    attrcb[SSH_FILEXFER_INDEX_CREATETIME].stat_mask	= SYSTEM_STAT_BTIME;

    /* modify time */

    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].code		= SSH_FILEXFER_ATTR_MODIFYTIME;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].shift		= SSH_FILEXFER_INDEX_MODIFYTIME;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].w_cb		= write_attr_modifytime_v04;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].r_cb		= read_attr_modifytime_v04;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].maxlength	= 8;
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].name		= "modifytime";
    attrcb[SSH_FILEXFER_INDEX_MODIFYTIME].stat_mask	= SYSTEM_STAT_MTIME;

    /* ACL */

    attrcb[SSH_FILEXFER_INDEX_ACL].code			= SSH_FILEXFER_ATTR_ACL;
    attrcb[SSH_FILEXFER_INDEX_ACL].shift		= SSH_FILEXFER_INDEX_ACL;
    attrcb[SSH_FILEXFER_INDEX_ACL].w_cb			= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_ACL].r_cb			= read_attr_acl_v06;
    attrcb[SSH_FILEXFER_INDEX_ACL].maxlength		= 0; /* unknown and not supported */
    attrcb[SSH_FILEXFER_INDEX_ACL].name			= "ACL";
    attrcb[SSH_FILEXFER_INDEX_ACL].stat_mask		= 0;

    /* attrib bits */

    attrcb[SSH_FILEXFER_INDEX_BITS].code		= SSH_FILEXFER_ATTR_BITS;
    attrcb[SSH_FILEXFER_INDEX_BITS].shift		= SSH_FILEXFER_INDEX_BITS;
    attrcb[SSH_FILEXFER_INDEX_BITS].w_cb		= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_BITS].r_cb		= read_attr_bits_v06;
    attrcb[SSH_FILEXFER_INDEX_BITS].maxlength		= 8;
    attrcb[SSH_FILEXFER_INDEX_BITS].name		= "attrib bits";
    attrcb[SSH_FILEXFER_INDEX_BITS].stat_mask		= 0;

    /* allocation size */

    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].code	= SSH_FILEXFER_ATTR_ALLOCATION_SIZE;
    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].shift	= SSH_FILEXFER_INDEX_ALLOCATION_SIZE;
    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].w_cb	= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].r_cb	= read_attr_alloc_size_v06;
    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].maxlength= 8;
    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].name	= "allocation size";
    attrcb[SSH_FILEXFER_INDEX_ALLOCATION_SIZE].stat_mask= 0;

    /* text hint */

    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].code		= SSH_FILEXFER_ATTR_TEXT_HINT;
    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].shift		= SSH_FILEXFER_INDEX_TEXT_HINT;
    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].w_cb		= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].r_cb		= read_attr_text_hint_v06;
    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].maxlength	= 1;
    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].name		= "text hint";
    attrcb[SSH_FILEXFER_INDEX_TEXT_HINT].stat_mask	= 0;

    /* mime type  */

    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].code		= SSH_FILEXFER_ATTR_MIME_TYPE;
    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].shift		= SSH_FILEXFER_INDEX_MIME_TYPE;
    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].w_cb		= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].r_cb		= read_attr_mime_type_v06;
    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].maxlength	= 0; /* variable width */
    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].name		= "mimetype";
    attrcb[SSH_FILEXFER_INDEX_MIME_TYPE].stat_mask	= 0;

    /* link count */

    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].code		= SSH_FILEXFER_ATTR_LINK_COUNT;
    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].shift		= SSH_FILEXFER_INDEX_LINK_COUNT;
    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].w_cb		= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].r_cb		= read_attr_link_count_v06;
    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].maxlength	= 4;
    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].name		= "link count";
    attrcb[SSH_FILEXFER_INDEX_LINK_COUNT].stat_mask	= 0;

    /* untranslated name */

    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].code		= SSH_FILEXFER_ATTR_UNTRANSLATED_NAME;
    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].shift		= SSH_FILEXFER_INDEX_UNTRANSLATED_NAME;
    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].w_cb		= NULL; /* not supported yet */
    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].r_cb		= read_attr_untranslated_name_v06;
    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].maxlength	= 0; /* variable width */
    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].name		= "untranslated name";
    attrcb[SSH_FILEXFER_INDEX_UNTRANSLATED_NAME].stat_mask	= 0;

    /* change time */

    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].code		= SSH_FILEXFER_ATTR_CHANGETIME;
    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].shift		= SSH_FILEXFER_INDEX_CHANGETIME;
    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].w_cb		= write_attr_changetime_v06;
    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].r_cb		= read_attr_changetime_v06;
    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].maxlength	= 8;
    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].name		= "changetime";
    attrcb[SSH_FILEXFER_INDEX_CHANGETIME].stat_mask	= SYSTEM_STAT_CTIME;

    /* extensions */

    attrcb[SSH_FILEXFER_INDEX_EXTENDED].code		= SSH_FILEXFER_ATTR_EXTENDED;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].shift		= SSH_FILEXFER_INDEX_EXTENDED;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].r_cb		= read_attr_extensions_v03;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].maxlength	= 0;
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].name		= "extended";
    attrcb[SSH_FILEXFER_INDEX_EXTENDED].stat_mask	= 0;

    actx->w_valid=(SSH_FILEXFER_ATTR_TYPE | SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_OWNERGROUP | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_CHANGETIME | SSH_FILEXFER_ATTR_SUBSECOND_TIMES);
    actx->w_count=10;
    actx->r_count=20;
    actx->r_valid=(SSH_FILEXFER_ATTR_TYPE | SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_OWNERGROUP | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_CREATETIME | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_SUBSECOND_TIMES |
		    SSH_FILEXFER_ATTR_ACL | SSH_FILEXFER_ATTR_BITS | SSH_FILEXFER_ATTR_ALLOCATION_SIZE | SSH_FILEXFER_ATTR_TEXT_HINT | SSH_FILEXFER_ATTR_MIME_TYPE | SSH_FILEXFER_ATTR_LINK_COUNT | SSH_FILEXFER_ATTR_UNTRANSLATED_NAME | SSH_FILEXFER_ATTR_CHANGETIME |
		    SSH_FILEXFER_ATTR_EXTENDED);

    /* nsec accesstime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].w_cb		= write_attr_accesstime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].w_cb		= read_attr_accesstime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].maxlength	= 4; /* nsec time use 4 bytes (=32 bits) */
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].name		= "nsec accesstime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_ATIME].stat_mask	= 0;

    /* nsec createtime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].w_cb		= NULL;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].w_cb		= read_attr_createtime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].maxlength	= 4; /* nsec time use 4 bytes (=32 bits) */
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].name		= "nsec createtime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_BTIME].stat_mask	= 0;

    /* nsec modifytime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].w_cb		= write_attr_modifytime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].r_cb		= read_attr_modifytime_n_v04;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].maxlength	= 4;
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].name		= "nsec modifytime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_MTIME].stat_mask	= 0;

    /* nsec changetime */

    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].code		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].shift		= 0;
    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].w_cb		= write_attr_changetime_n_v06;
    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].r_cb		= read_attr_changetime_n_v06;
    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].maxlength	= 4;
    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].name		= "nsec changetime";
    attrcb[SSH_FILEXFER_INDEX_NSEC_CTIME].stat_mask	= 0;

}

/*
    prepare the reading or writing of attributes following the rules in

    https://datatracker.ietf.org/doc/html/draft-ietf-secsh-filexfer-13#section-7


       uint32   valid-attribute-flags
       byte     type                   always present
       uint64   size                   if flag SIZE
       uint64   allocation-size        if flag ALLOCATION_SIZE
       string   owner                  if flag OWNERGROUP
       string   group                  if flag OWNERGROUP
       uint32   permissions            if flag PERMISSIONS
       int64    atime                  if flag ACCESSTIME
       uint32   atime-nseconds            if flag SUBSECOND_TIMES
       int64    createtime             if flag CREATETIME
       uint32   createtime-nseconds       if flag SUBSECOND_TIMES
       int64    mtime                  if flag MODIFYTIME
       uint32   mtime-nseconds            if flag SUBSECOND_TIMES
       int64    ctime                  if flag CTIME
       uint32   ctime-nseconds            if flag SUBSECOND_TIMES
       string   acl                    if flag ACL
       uint32   attrib-bits            if flag BITS
       uint32   attrib-bits-valid      if flag BITS
       byte     text-hint              if flag TEXT_HINT
       string   mime-type              if flag MIME_TYPE
       uint32   link-count             if flag LINK_COUNT
       string   untranslated-name      if flag UNTRANSLATED_NAME
       uint32   extended-count         if flag EXTENDED
       extension-pair extensions


	NOTE:
	    - specific attributes are only present if bit in the first 4 bytes ("flags") is set for that property
	    - the order is important

*/

void parse_attributes_v06(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{

    /* there is always a type */

    (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_TYPE);
    if (r->todo & SSH_FILEXFER_ATTR_SIZE) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_SIZE);
    if (r->todo & SSH_FILEXFER_ATTR_ALLOCATION_SIZE) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_ALLOCATION_SIZE);
    if (r->todo & SSH_FILEXFER_ATTR_OWNERGROUP) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_OWNERGROUP);
    if (r->todo & SSH_FILEXFER_ATTR_PERMISSIONS) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_PERMISSIONS);

    if (r->todo & SSH_FILEXFER_ATTR_ACCESSTIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_ACCESSTIME);
	if (r->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_ATIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_CREATETIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_CREATETIME);
	if (r->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_BTIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_MODIFYTIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_MODIFYTIME);
	if (r->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_MTIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_CHANGETIME) {

	(* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_CHANGETIME);
	if (r->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_NSEC_CTIME);

    }

    if (r->todo & SSH_FILEXFER_ATTR_ACL) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_ACL);
    if (r->todo & SSH_FILEXFER_ATTR_BITS) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_BITS);
    if (r->todo & SSH_FILEXFER_ATTR_TEXT_HINT) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_TEXT_HINT);
    if (r->todo & SSH_FILEXFER_ATTR_MIME_TYPE) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_MIME_TYPE);
    if (r->todo & SSH_FILEXFER_ATTR_LINK_COUNT) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_LINK_COUNT);
    if (r->todo & SSH_FILEXFER_ATTR_UNTRANSLATED_NAME) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_UNTRANSLATED_NAME);
    if (r->todo & SSH_FILEXFER_ATTR_EXTENDED) (* r->parse_attribute)(actx, buffer, r, stat, SSH_FILEXFER_INDEX_EXTENDED);

}

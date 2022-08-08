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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v05.h"
#include "sftp/attr-context.h"

#include "read-attr-v03.h"
#include "read-attr-v04.h"
#include "read-attr-v05.h"

#include "datatypes/ssh-uint.h"

/* more information:

   https://tools.ietf.org/html/draft-ietf-secsh-filexfer-05#section-5 */

static unsigned int type_mapping[10]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0, S_IFSOCK, S_IFCHR, S_IFBLK, S_IFIFO};

void read_attr_type_v05(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned char tmp=(* buffer->ops->rw.read.read_uchar)(buffer);
    unsigned int type=(tmp<10) ? type_mapping[tmp] : 0;
    set_type_system_stat(stat, type);
    logoutput_debug("read_attr_type_v05: type %i", type);
}

void read_attr_bits_v05(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{
    unsigned int bits=(* buffer->ops->rw.read.read_uint32)(buffer);

    /* 20211018: what to do with it ? */
}

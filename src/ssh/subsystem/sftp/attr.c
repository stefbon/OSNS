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
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "osns_sftp_subsystem.h"
#include "init.h"

static unsigned char get_sftp_subsystem_protocol_version(struct attr_context_s *ctx)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *)((char *) ctx - offsetof(struct sftp_subsystem_s, attrctx));
    logoutput_debug("get_sftp_subsystem_protocol_version");
    return get_sftp_protocol_version(sftp);
}

void init_sftp_subsystem_attr_context(struct sftp_subsystem_s *sftp)
{
    struct attr_context_s *actx=&sftp->attrctx;

    init_attr_context(actx, ATTR_CONTEXT_FLAG_SERVER, NULL, &sftp->mapping);
    actx->get_sftp_protocol_version=get_sftp_subsystem_protocol_version;
}

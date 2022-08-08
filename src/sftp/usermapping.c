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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-resources.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "init.h"

void get_sftp_usermapping(struct sftp_client_s *sftp)
{
    struct net_idmapping_s *mapping=sftp->mapping;
    unsigned int flag=0;

    if (get_sftp_protocol_version(sftp)>3) {

	flag |= NET_IDMAPPING_FLAG_MAPBYNAME;

    } else {

	flag |= NET_IDMAPPING_FLAG_MAPBYID;

    }

    if ((* mapping->setup)(mapping, flag)>0) {

	logoutput("get_sftp_usermapping: id's %s", (mapping->flags & NET_IDMAPPING_FLAG_SHARED) ? "shared" : "nonshared");

    } else {

	logoutput("get_sftp_usermapping: already setup");

    }

}


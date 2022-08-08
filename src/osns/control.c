/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-connection.h"

#include "osns-protocol.h"
#include "receive.h"
#include "send.h"

unsigned int write_osns_control_info(char *data, unsigned int len, struct osns_control_info_s *info)
{
    unsigned int pos=0;

    if (info->code==OSNS_CONTROL_TYPE_OSNS_SOCKET) {

	if (data) {

	    if (len>10) {

		store_uint16(&data[pos], info->code);
		pos+=2;
		store_uint32(&data[pos], info->info.osns_socket.type);
		pos+=4;
		store_uint32(&data[4], info->info.osns_socket.flags);
		pos+=4;

	    }

	} else {

	    pos=10;

	}

    }

    return pos;
}

int read_osns_control_info(char *data, unsigned int len, struct osns_control_info_s *info)
{
    unsigned int pos=0;

    if (len>=2) {

	info->code=get_uint16(&data[pos]);
	pos+=2;

	if (info->code==OSNS_CONTROL_TYPE_OSNS_SOCKET) {

	    if (len>10) {

		info->info.osns_socket.type=get_uint32(&data[pos]);
		pos+=4;
		info->info.osns_socket.flags=get_uint32(&data[4]);
		pos+=4;

	    }

	}

    }

    return pos;
}

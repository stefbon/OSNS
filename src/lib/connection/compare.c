/*
 
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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
#include "libosns-error.h"
#include "connection.h"

static int _compare_network_conn_ip(struct connection_s *a, struct connection_s *b)
{
    struct network_peer_s pa;
    struct network_peer_s pb;
    int result=-1;

    memset(&pa, 0, sizeof(struct network_peer_s));
    memset(&pb, 0, sizeof(struct network_peer_s));
    pa.host.flags = HOST_ADDRESS_FLAG_HOSTNAME;
    pb.host.flags = HOST_ADDRESS_FLAG_HOSTNAME;

    if (get_network_peer_properties(&a->sock, &pa, "remote")==0 &&
	get_network_peer_properties(&b->sock, &pb, "remote")==0) {

	if (strcmp(pa.host.hostname, pb.host.hostname)==0) result=0;

    }

    return result;

}

int compare_network_connection(struct connection_s *a, struct connection_s *b, unsigned int flags)
{

    if (flags & CONNECTION_COMPARE_HOST) {

	return _compare_network_conn_ip(a, b);

    }

    return -1;

}

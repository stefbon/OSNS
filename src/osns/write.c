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
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-network.h"
#include "libosns-eventloop.h"
#include "libosns-socket.h"

#include "osns-protocol.h"

static int send_data_cb_default(struct osns_socket_s *sock, char *data, unsigned int size, void *ptr)
{
    struct iovec iov[1];
    struct msghdr msg;

    iov[0].iov_base=(void *) data;
    iov[0].iov_len=(size_t) size;

    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;
    msg.msg_control=NULL;
    msg.msg_controllen=0;
    msg.msg_flags=0;

    return (* sock->sops.connection.sendmsg)(sock, &msg);
}

int write_osns_socket(struct osns_socket_s *sock, char *data, unsigned int size, int (* send_cb)(struct osns_socket_s *sock, char *data, unsigned int size, void *ptr), void *ptr)
{
    int byteswritten=-1;

    if (send_cb==NULL) send_cb=send_data_cb_default;

    if (sock->event.type==SOCKET_EVENT_TYPE_BEVENT) {
	struct bevent_s *bevent=sock->event.link.bevent;
	struct bevent_write_data_s wdata;

	wdata.flags=0;
	wdata.data=data;
	wdata.size=size;
	wdata.byteswritten=0;
	set_system_time(&wdata.timeout, 4, 0);
	wdata.ptr=ptr;
	init_generic_error(&wdata.error);

	writesocket:
	byteswritten=write_socket_signalled(bevent, &wdata, send_cb);

	if (byteswritten>0) {

	    if (byteswritten < size) {

		logoutput_debug("write_osns_socket: bytes written %u left %u", (unsigned int) byteswritten, size - byteswritten);

	    }

	} else if (byteswritten<0) {

	    logoutput_debug("write_osns_socket: error");

	} else {

	    /* byteswritten==0 */
	    logoutput_debug("write_osns_socket: connection closed");

	}

    }

    return byteswritten;

}

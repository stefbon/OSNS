/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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

#include <fcntl.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "lib/system/stat.h"

#include "socket.h"
#include "common.h"
#include "utils.h"

int send_data_cb_socket(struct osns_socket_s *sock, char *data, unsigned int size, void *ptr)
{
    struct iovec iov[1];
    struct msghdr msg;
    struct osns_socket_s *tosend=(struct osns_socket_s *) ptr;

#ifdef __linux__

    int fd=(* tosend->get_unix_fd)(tosend);
    unsigned int tmp=sizeof(int);
    union {
	char 			buffer[CMSG_SPACE(tmp)];
	struct cmsghdr		allign;
    } uhlp;
    struct cmsghdr *cmsg=NULL;

    memset(&uhlp, 0, sizeof(uhlp));

    logoutput_debug("send_data_cb_socket: fd %i", fd);

#endif

    iov[0].iov_base=(void *) data;
    iov[0].iov_len=(size_t) size;

    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;
    msg.msg_flags=0;

#ifdef __linux__

    msg.msg_control=(void *) uhlp.buffer;
    msg.msg_controllen=sizeof(uhlp.buffer);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(tmp);

    memcpy(CMSG_DATA(cmsg), &fd, tmp);

#else

    msg.msg_control=NULL;
    msg.msg_controllen=0;

#endif

    return (* sock->sops.connection.sendmsg)(sock, &msg);
}

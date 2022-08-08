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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

#include "socket.h"

int socket_connect_dummy(struct system_socket_s *sock)
{
    return -1;
}

int socket_recvsend_dummy(struct system_socket_s *sock, char *buffer, unsigned int size, unsigned int flags)
{
    return -1;
}

int socket_readwrite_dummy(struct system_socket_s *sock, char *buffer, unsigned int size)
{
    return -1;
}

int socket_writevreadv_dummy(struct system_socket_s *sock, struct iovec *iov, unsigned int count)
{
    return -1;
}

int socket_recvmsg_dummy(struct system_socket_s *sock, struct msghdr *msg)
{
    return -1;
}

int socket_sendmsg_dummy(struct system_socket_s *sock, const struct msghdr *msg)
{
    return -1;
}

int socket_read_common(struct system_socket_s *sock, char *buffer, unsigned int size)
{
    int result=-1;

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);

    result=(int) read(fdc, (void *) buffer, (size_t) size);

    if (result==-1) {

	logoutput_debug("socket_read_common: error %u:%s", errno, strerror(errno));

    } else {

	logoutput_debug("socket_read_common: fd %i size %u", fdc, size);


    }

#endif

    return result;
}

int socket_write_common(struct system_socket_s *sock, char *buffer, unsigned int size)
{
    int result=-1;

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);

    errno=0;
    result=write(fdc, buffer, size);

#endif

    return result;
}


int socket_recv_common(struct system_socket_s *sock, char *buffer, unsigned int size, int flags)
{
    int result=-1;

#ifdef __linux__

    flags &= (MSG_DONTWAIT | MSG_PEEK | MSG_TRUNC | MSG_WAITALL);

    int fdc=(* sock->sops.get_unix_fd)(sock);
    logoutput_debug("socket_recv_common: fd %i size %u flags %i", fdc, size, flags);
    result=recv(fdc, buffer, size, flags);

#endif

    return result;
}

int socket_send_common(struct system_socket_s *sock, char *buffer, unsigned int size, int flags)
{
    int result=-1;

    flags = 0; /* 20220111: ignore flags */

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);

    errno=0;
    result=send(fdc, buffer, size, flags);

#endif

    return result;
}

int socket_writev_common(struct system_socket_s *sock, struct iovec *iov, unsigned int count)
{
    int result=-1;

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);
    errno=0;
    result=writev(fdc, iov, count);

    if (result==-1) {

	logoutput_debug("socket_writev_common: fd %i count %u error %u", fdc, count, errno);

    } else {

	logoutput_debug("socket_writev_common: fd %i count %u result %u", fdc, count, (unsigned int) result);

    }

#endif

    return result;
}

int socket_readv_common(struct system_socket_s *sock, struct iovec *iov, unsigned int count)
{
    int result=-1;

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);

    errno=0;
    result=readv(fdc, iov, count);

#endif

    return result;
}

int socket_recvmsg_common(struct system_socket_s *sock, struct msghdr *msg)
{
    int result=-1;

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);

    errno=0;
    result=recvmsg(fdc, msg, 0);

#endif

    return result;
}

int socket_sendmsg_common(struct system_socket_s *sock, const struct msghdr *msg)
{
    int result=-1;

#ifdef __linux__

    int fdc=(* sock->sops.get_unix_fd)(sock);

    errno=0;
    result=sendmsg(fdc, msg, 0);

#endif

    return result;
}


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
#include "connection.h"
#include "device.h"
#include "file.h"
#include "directory.h"
#include "utils.h"

static int cb_open_socket_default(struct osns_socket_s *sock, unsigned int flags)
{
    return -1;
}

static int cb_getsockopt_default(struct osns_socket_s *sock, struct generic_socket_option_s *option)
{

#ifdef __linux__

    int fd=(* sock->get_unix_fd)(sock);
    return ((fd>=0) ? getsockopt(fd, option->level, option->type, option->value, &option->len) : -1);

#else

    return -1;

#endif

}

static int cb_setsockopt_default(struct osns_socket_s *sock, struct generic_socket_option_s *option)
{

#ifdef __linux__

    int fd=(* sock->get_unix_fd)(sock);
    return ((fd>=0) ? setsockopt(fd, option->level, option->type, option->value, option->len) : -1);

#else

    return -1;

#endif

}

static void cb_close_default(struct osns_socket_s *sock)
{

#ifdef __linux__
    int fd=(* sock->get_unix_fd)(sock);

    if (fd>=0) {

	close(fd);
	(* sock->set_unix_fd)(sock, -1);
	(* sock->change)(sock, SOCKET_CHANGE_OP_CLOSE);

    }

#endif

}

static int cb_get_unix_fd_default(struct osns_socket_s *sock)
{
#ifdef __linux__

    return sock->fd;

#else

    return -1;

#endif
}

static void cb_set_unix_fd_default(struct osns_socket_s *sock, int fd)
{
#ifdef __linux__

    int keep=sock->fd;

    sock->fd=fd;
    if (keep != fd) (* sock->change)(sock, SOCKET_CHANGE_OP_SET);

#endif
}

static void cb_change_default(struct osns_socket_s *sock, unsigned int what)
{
    
}

static unsigned char countbitsset(unsigned int value)
{
    unsigned char count=0;

    while (value) {

	count += (value & 1);
	value >>= 1;

    }

    return count;
}


/* initialization */

void init_osns_socket(struct osns_socket_s *sock, unsigned int type, unsigned int flags)
{
    unsigned int stype=0;
    unsigned int atype=0;

    logoutput_debug("init_osns_socket: type %u flags %u", type, flags);
    memset(sock, 0, sizeof(struct osns_socket_s));

    if (countbitsset(flags & (OSNS_SOCKET_FLAG_RDWR | OSNS_SOCKET_FLAG_WRONLY))>1) {

	logoutput_error("init_osns_socket: invalid flags ... RDWR and WRONLY cannot be set both");

    }

    switch (type) {

	case OSNS_SOCKET_TYPE_CONNECTION:
	{

	    if (flags & (OSNS_SOCKET_FLAG_NET | OSNS_SOCKET_FLAG_LOCAL)) {
		unsigned int tmp=0;
		unsigned char count=0;

		tmp=(OSNS_SOCKET_FLAG_IPv4 | OSNS_SOCKET_FLAG_IPv6);
		count=countbitsset(flags & tmp);

		if (flags & OSNS_SOCKET_FLAG_NET) {

		    if (count==0 || count>1) {

			/* at least one (and not more) has to be set */

			logoutput_error("init_osns_socket: invalid flags ... cannot continue");
			return;

		    }

		} else if (flags & OSNS_SOCKET_FLAG_LOCAL) {

		    /* local socket and ip flags are invalid */

		    if (count>0) {

			logoutput_error("init_osns_socket: invalid flags ... local socket and ip flags are incompatible");
			return;

		    }

		}

		tmp |= (OSNS_SOCKET_FLAG_NET | OSNS_SOCKET_FLAG_LOCAL | OSNS_SOCKET_FLAG_UDP | OSNS_SOCKET_FLAG_ENDPOINT | OSNS_SOCKET_FLAG_SERVER | OSNS_SOCKET_FLAG_RDWR | OSNS_SOCKET_FLAG_WRONLY);

		if (flags & ~tmp) {

		    /* flags set which do not fit with connection */
		    logoutput_error("init_osns_socket: invalid flags ... %u are set which are invalid with a socket for connections", (flags & ~tmp));
		    return;

		}

		if ((flags & OSNS_SOCKET_FLAG_ENDPOINT) && (flags & OSNS_SOCKET_FLAG_WRONLY)) {

		    /* endpoint socket is always rdwr, never rdonly or wronly  */
		    logoutput_error("init_osns_socket: invalid flags ... WRONLY flag is set and is incompatible with the ENDPOINT flag");
		    return;

		}

	    } else {

		/* one flag NET or LOCAL has to be set */
		logoutput_error("init_osns_socket: invalid flags ... one flag NET or LOCAL has to be set");
		return;

	    }

	    break;

	}

	case OSNS_SOCKET_TYPE_DEVICE:
	{
	    unsigned int tmp=0;
	    unsigned char count=0;

	    tmp=(OSNS_SOCKET_FLAG_CHAR_DEVICE | OSNS_SOCKET_FLAG_BLOCK_DEVICE);
	    count=countbitsset(flags & tmp);

	    if (count==0 || count>1) {

		/* at least one (and not more) has to be set */

		logoutput_error("init_osns_socket: invalid flags ... one of CHAR_DEVICE or BLOCK_DEVICE has to be set");
		return;

	    }


	    tmp |= (OSNS_SOCKET_FLAG_RDWR | OSNS_SOCKET_FLAG_WRONLY);

	    if (flags & ~tmp) {

		/* flags set which do not fit with system */
		logoutput_error("init_osns_socket: invalid flags ... %u are set which are invalid with a system socket", (flags & ~tmp));
		return;

	    }

	    break;

	}

	case OSNS_SOCKET_TYPE_FILESYSTEM:
	{
	    unsigned int tmp=0;
	    unsigned char count=0;

	    tmp=(OSNS_SOCKET_FLAG_FILE | OSNS_SOCKET_FLAG_DIR);
	    count=countbitsset(flags & tmp);

	    if (count==0 || count>1) {

		/* at least one (and not more) has to be set */

		logoutput_error("init_osns_socket: invalid flags ... one of FILE and DIR has to be set");
		return;

	    }

	    tmp |= (OSNS_SOCKET_FLAG_RDWR | OSNS_SOCKET_FLAG_WRONLY);

	    if (flags & ~tmp) {

		/* flags set which do not fit with system */
		logoutput_error("init_osns_socket: invalid flags ... %u are set which are invalid with a system socket", (flags & ~tmp));
		return;

	    }

	    break;

	}

	default:

	{
	    logoutput_error("init_osns_socket: type not reckognized, must be one of CONNECTION, SYSTEM or FILESYSTEM: invalid ... cannot continue");
	    return;
	}

    }

    sock->status=SOCKET_STATUS_INIT;
    sock->type=type;
    sock->flags=flags;

    /* context */

    /* backend - system specific (Linux, W32/64, OSX, ...)*/

#ifdef __linux__

    sock->fd=-1;
    sock->pid=0;

#endif

    /* socket ops -  defaults */

    sock->getsockopt=cb_getsockopt_default;
    sock->setsockopt=cb_setsockopt_default;
    sock->close=cb_close_default;

#ifdef __linux__

    sock->get_unix_fd=cb_get_unix_fd_default;
    sock->set_unix_fd=cb_set_unix_fd_default;

#endif
    sock->change=cb_change_default;

    switch (sock->type) {

	case OSNS_SOCKET_TYPE_CONNECTION:

	    init_osns_connection_socket(sock);
	    break;

	case OSNS_SOCKET_TYPE_DEVICE:

	    init_osns_device_socket(sock);
	    break;


	case OSNS_SOCKET_TYPE_FILESYSTEM:

	    if (sock->flags & OSNS_SOCKET_FLAG_FILE) {

		init_osns_file_socket(sock);

	    } else if (sock->flags & OSNS_SOCKET_FLAG_DIR) {

		init_osns_directory_socket(sock);

	    }

	    break;

	default:


    }

}

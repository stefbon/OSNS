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
#include "system.h"
#include "utils.h"

static void cb_change_backend_default(struct system_socket_s *sock, unsigned char action)
{
    /* default does nothing since the backend/context (->eventloop) is not known here */
}

static int cb_getsockopt_default(struct system_socket_s *sock, struct generic_socket_option_s *option)
{

#ifdef __linux__

    int fd=(* sock->sops.get_unix_fd)(sock);

    return ((fd>=0) ? getsockopt(fd, option->level, option->type, option->value, &option->len) : -1);

#else

    return -1;

#endif

}

static int cb_setsockopt_default(struct system_socket_s *sock, struct generic_socket_option_s *option)
{

#ifdef __linux__

    int fd=(* sock->sops.get_unix_fd)(sock);

    return ((fd>=0) ? getsockopt(fd, option->level, option->type, option->value, &option->len) : -1);

#else

    return -1;

#endif

}

static void cb_close_default(struct system_socket_s *sock)
{
    struct system_socket_backend_s *backend=&sock->backend;

#ifdef __linux__
    int fd=(* sock->sops.get_unix_fd)(sock);

    if (fd>=0) {

	close(fd);
	(* sock->sops.set_unix_fd)(sock, -1);

    }

#endif

    (* backend->cb_change)(sock, SOCKET_BACKEND_ACTION_CLOSE);

}

static int cb_get_unix_fd_default(struct system_socket_s *sock)
{
#ifdef __linux__

    return sock->backend.fd;

#else

    return -1;

#endif
}

static void cb_set_unix_fd_default(struct system_socket_s *sock, int fd)
{
#ifdef __linux__

    sock->backend.fd=fd;

    if (sock->flags & SYSTEM_SOCKET_FLAG_NOOPEN) {

	if (sock->type & SYSTEM_SOCKET_TYPE_SYSTEM) {

	    set_unix_fd_system(sock);

	}

	sock->flags &= ~SYSTEM_SOCKET_FLAG_NOOPEN;

    }

#endif
}

static unsigned char countbitsset(unsigned int value)
{
    unsigned char count=0;

    logoutput_debug("countbitsset: value %u", value);

    while (value) {

	count += (value & 1);
	value >>= 1;

    }

    return count;
}


/* initialization */

void init_system_socket(struct system_socket_s *sock, unsigned int type, unsigned int flags, struct fs_location_path_s *path)
{
    unsigned int stype=0;
    unsigned int atype=0;

    logoutput_debug("init_system_socket: type %u flags %u", type, flags);
    memset(sock, 0, sizeof(struct system_socket_s));

    switch (type) {

	case SYSTEM_SOCKET_TYPE_LOCAL:
	case SYSTEM_SOCKET_TYPE_NET:

	    type |= SYSTEM_SOCKET_TYPE_CONNECTION;
	    break;

	case SYSTEM_SOCKET_TYPE_CHAR_DEVICE:
	case SYSTEM_SOCKET_TYPE_BLOCK_DEVICE:
	case SYSTEM_SOCKET_TYPE_SYSTEM_DEVICE:
	case SYSTEM_SOCKET_TYPE_SYSTEM_FILE:

	    type |= SYSTEM_SOCKET_TYPE_SYSTEM;
	    break;

	case SYSTEM_SOCKET_TYPE_FILE:
	case SYSTEM_SOCKET_TYPE_DIR:

	    type |= SYSTEM_SOCKET_TYPE_FILESYSTEM;
	    break;

    }

    stype=(type & SYSTEM_SOCKET_TYPE_MASK);
    atype=(type & ~SYSTEM_SOCKET_TYPE_MASK);

    logoutput_debug("init_system_socket: stype %u atype %u flags %u", stype, atype, flags);

    switch (stype) {

	case SYSTEM_SOCKET_TYPE_CONNECTION:
	{
	    unsigned int tmp=(SYSTEM_SOCKET_TYPE_LOCAL | SYSTEM_SOCKET_TYPE_NET);

	    /* a connection is possible:
		- a connection created after accepted by a socket endpoint, so NETWORK and LOCAL/UNIX
		- a system connection for example with char devices */

	    if (atype & ~tmp) {

		logoutput_error("init_system_socket: invalid type ... cannot continue");
		return;

	    } else {
		unsigned char count=countbitsset((atype & tmp));

		/* one of the bits has to be set, but not more than one */

		if (count==0 || count>1) {

		    logoutput_error("init_system_socket: invalid type ... cannot continue (count=%u)", count);
		    return;

		}

	    }

	    sock->type=(SYSTEM_SOCKET_TYPE_CONNECTION | (atype & tmp));
	    break;

	}
	case SYSTEM_SOCKET_TYPE_SYSTEM:
	{
	    unsigned int tmp=(SYSTEM_SOCKET_TYPE_CHAR_DEVICE | SYSTEM_SOCKET_TYPE_BLOCK_DEVICE | SYSTEM_SOCKET_TYPE_SYSTEM_DEVICE | SYSTEM_SOCKET_TYPE_SYSTEM_FILE);

	    if (atype & ~tmp) {

		logoutput_error("init_system_socket: invalid type ... cannot continue");
		return;

	    } else {
		unsigned char count=countbitsset((atype & tmp));

		/* one of the bits has to be set, but not both */

		if (count==0 || count>1) {

		    logoutput_error("init_system_socket: too much types set ... cannot continue");
		    return;

		}

	    }

	    sock->type=(SYSTEM_SOCKET_TYPE_SYSTEM | (atype & tmp));
	    break;

	}

	case SYSTEM_SOCKET_TYPE_FILESYSTEM:
	{
	    unsigned int tmp=(SYSTEM_SOCKET_TYPE_FILE | SYSTEM_SOCKET_TYPE_DIR);

	    if (atype & ~tmp) {

		logoutput_error("init_system_socket: invalid type ... cannot continue");
		return;

	    } else {
		unsigned char count=countbitsset((atype & tmp));

		/* one of the bits has to be set, but not more than one */

		if (count==0 || count>1) {

		    logoutput_error("init_system_socket: invalid flags ... cannot continue");
		    return;

		}

	    }

	    sock->type=(SYSTEM_SOCKET_TYPE_FILESYSTEM | (atype & tmp));
	    break;

	}

	default:

	{
	    logoutput_error("init_system_socket: tytpe not reckognized, must be one of LOCAL, NET or SYSTEM: invalid ... cannot continue");
	    return;
	}

    }

    logoutput_debug("init_system_socket: sock type %u", sock->type);

    if (sock->type & SYSTEM_SOCKET_TYPE_LOCAL) {
	unsigned int valid = (SYSTEM_SOCKET_FLAG_UDP | SYSTEM_SOCKET_FLAG_SERVER | SYSTEM_SOCKET_FLAG_ENDPOINT | SYSTEM_SOCKET_FLAG_NOOPEN);

	if (flags & ~valid) {

	    logoutput_error("init_system_socket: invalid flags ... cannot continue (A)");
	    return;

	}

	sock->flags |= (flags & valid);

    } else if (sock->type & SYSTEM_SOCKET_TYPE_NET) {
	unsigned int valid=(SYSTEM_SOCKET_FLAG_UDP | SYSTEM_SOCKET_FLAG_IPv4 | SYSTEM_SOCKET_FLAG_IPv6 | SYSTEM_SOCKET_FLAG_SERVER | SYSTEM_SOCKET_FLAG_ENDPOINT | SYSTEM_SOCKET_FLAG_NOOPEN);
	unsigned char count=countbitsset((flags & (SYSTEM_SOCKET_FLAG_IPv4 | SYSTEM_SOCKET_FLAG_IPv6)));

	/* one of the bits has to be set, but not both */

	if (count==0 || count>1) {

	    logoutput_error("init_system_socket: invalid flags ... cannot continue (B)");
	    return;

	}

	if (flags & ~valid) {

	    logoutput_error("init_system_socket: invalid flags ... cannot continue (C)");
	    return;

	}

	sock->flags |= (flags & valid);

    } else if (sock->type & SYSTEM_SOCKET_TYPE_SYSTEM) {
	unsigned int valid=(SYSTEM_SOCKET_FLAG_NOOPEN | SYSTEM_SOCKET_FLAG_RDWR | SYSTEM_SOCKET_FLAG_WRONLY);

	if (flags & ~valid) {

	    logoutput_error("init_system_socket: invalid flags ... cannot continue (D)");
	    return;

	}

	sock->flags |= (flags & valid);

    // } else if (sock->type & SYSTEM_SOCKET_TYPE_FILESYSTEM) {
	//unsigned int valid=(SYSTEM_SOCKET_FLAG_RDWR | SYSTEM_SOCKET_FLAG_WRONLY);

	//if (flags & ~valid) {

	    //logoutput_error("init_system_socket: invalid flags ... cannot continue (E)");
	    //return;

	//}

	//sock->flags |= (flags & valid);

    }

    sock->status=SOCKET_STATUS_INIT;

    /* context */

    /* backend - system specific (Linux, W32/64, OSX, ...)*/

#ifdef __linux__

    sock->backend.fd=-1;
    sock->backend.pid=0;

#endif

    sock->backend.ptr=NULL;
    sock->backend.cb_change=cb_change_backend_default;

    /* socket ops -  defaults */

    sock->sops.getsockopt=cb_getsockopt_default;
    sock->sops.setsockopt=cb_setsockopt_default;
    sock->sops.close=cb_close_default;
    sock->sops.get_unix_fd=cb_get_unix_fd_default;
    sock->sops.set_unix_fd=cb_set_unix_fd_default;

    switch (stype) {

	case SYSTEM_SOCKET_TYPE_CONNECTION:

	    init_system_socket_connection(sock);
	    break;

	case SYSTEM_SOCKET_TYPE_SYSTEM:

	    init_system_socket_system(sock, path);
	    break;

//	case SYSTEM_SOCKET_TYPE_FILESYSTEM:

//	    init_system_socket_filesystem(sock, flags, path);
//	    break

	default:


    }

}

char *get_name_system_socket(struct system_socket_s *sock, const char *what)
{
    char *name="";

    if (sock->type & SYSTEM_SOCKET_TYPE_CONNECTION) {

	if (strcmp(what, "type")==0) {

	    name="connection";

	} else if (strcmp(what, "subtype")==0) {

	    if (sock->type & SYSTEM_SOCKET_TYPE_LOCAL) {

		name="local";

	    } else if (sock->type & SYSTEM_SOCKET_TYPE_NET) {

		name="net";

	    }

	}

    } else if (sock->type & SYSTEM_SOCKET_TYPE_SYSTEM) {

	if (strcmp(what, "type")==0) {

	    name="system";

	} else if (strcmp(what, "subtype")==0) {

	    if (sock->type & SYSTEM_SOCKET_TYPE_CHAR_DEVICE) {

		name="char-device";

	    } else if (sock->type & SYSTEM_SOCKET_TYPE_BLOCK_DEVICE) {

		name="block-device";

	    } else if (sock->type & SYSTEM_SOCKET_TYPE_SYSTEM_DEVICE) {

		name="system-device";

	    } else if (sock->type & SYSTEM_SOCKET_TYPE_SYSTEM_FILE) {

		name="system-file";

	    }

	}

    } else if (sock->type & SYSTEM_SOCKET_TYPE_FILESYSTEM) {

	if (strcmp(what, "type")==0) {

	    name="filesystem";

	} else if (strcmp(what, "subtype")==0) {

	    if (sock->type & SYSTEM_SOCKET_TYPE_FILE) {

		name="file";

	    } else if (sock->type & SYSTEM_SOCKET_TYPE_DIR) {

		name="dir";

	    }

	}

    }

    return name;
}

unsigned int get_type_system_socket(char *type, char *subtype)
{
    unsigned int stype=0;

    if (strcmp(type, "connection")==0) {

	stype = SYSTEM_SOCKET_TYPE_CONNECTION;

	if (subtype) {

	    if (strcmp(subtype, "unix")==0 || strcmp(subtype, "local")==0) {

		stype |= SYSTEM_SOCKET_TYPE_LOCAL;

	    } else if (strcmp(subtype, "net")==0) {

		stype |= SYSTEM_SOCKET_TYPE_NET;

	    }

	}

    } else if (strcmp(type, "system")==0) {

	stype = SYSTEM_SOCKET_TYPE_SYSTEM;

	if (subtype) {

	    if (strcmp(subtype, "char-device")==0) {

		stype |= SYSTEM_SOCKET_TYPE_CHAR_DEVICE;

	    } else if (strcmp(subtype, "block-device")==0) {

		stype |= SYSTEM_SOCKET_TYPE_BLOCK_DEVICE;

	    } else if (strcmp(subtype, "system-device")==0) {

		stype |= SYSTEM_SOCKET_TYPE_SYSTEM_DEVICE;

	    } else if (strcmp(subtype, "system-file")==0) {

		stype |= SYSTEM_SOCKET_TYPE_SYSTEM_FILE;

	    }

	}

    } else if (strcmp(type, "filesystem")==0) {

	stype = SYSTEM_SOCKET_TYPE_FILESYSTEM;

	if (subtype) {

	    if (strcmp(subtype, "dir")==0) {

		stype |= SYSTEM_SOCKET_TYPE_DIR;

	    } else if (strcmp(subtype, "file")==0) {

		stype |= SYSTEM_SOCKET_TYPE_FILE;

	    }

	}

    }

    logoutput_debug("get_type_system_socket: name %s:%s type %u", type, subtype, stype);
    return stype;

}


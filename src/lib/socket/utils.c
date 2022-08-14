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

#ifdef __linux__

#include <pwd.h>
#include <grp.h>

#endif

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

#include "lib/system/stat.h"
#include "socket.h"
#include "utils.h"

int translate_osns_socket_flags(unsigned int flags)
{
    int openflags=0;

    if (flags & OSNS_SOCKET_FLAG_RDWR) {

	openflags = O_RDWR;

    } else if (flags & OSNS_SOCKET_FLAG_WRONLY) {

	openflags = O_WRONLY;

    } else {

	openflags = O_RDONLY;

    }

    /* more ? */
    return openflags;
}

int check_status_hlpr(unsigned int status, unsigned int set, unsigned int notset)
{
    if (set && ((status & set) != set)) return -1;
    if (notset && (status & notset)) return -1;
    return 0;
}

unsigned int get_status_osns_socket(struct osns_socket_s *sock)
{
    struct generic_socket_option_s option;
    int error=0;

    if (sock==NULL) return EINVAL;

    option.level=SOL_SOCKET;
    option.type=SO_ERROR;
    option.value=(char *) &error;
    option.len=sizeof(int);

    if ((* sock->getsockopt)(sock, &option)==0) {

	logoutput("get_status_osns_socket: got error %i (%s)", error, strerror(error));

    } else {

	error=errno;
	logoutput("get_status_osns_socket: error %i (%s)", errno, strerror(errno));

    }

    return (unsigned int) abs(error);

}

void set_osns_socket_nonblocking(struct osns_socket_s *sock)
{
#ifdef __linux__
    int fd=(* sock->get_unix_fd)(sock);

    if (fd>=0) {

	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, (flags | O_NONBLOCK));

    }

#endif
}

void unset_osns_socket_nonblocking(struct osns_socket_s *sock)
{
#ifdef __linux__
    int fd=(* sock->get_unix_fd)(sock);

    if (fd>=0) {

	int flags = fcntl(fd, F_GETFL, 0);

	if (flags & O_NONBLOCK) {

	    flags &= ~O_NONBLOCK;
	    fcntl(fd, F_SETFL, flags);

	}

    }

#endif
}

unsigned char socket_connection_error(unsigned int error)
{
    unsigned char result=0;

    switch (error) {

	case EBADF:
	case ENETDOWN:
	case ENETUNREACH:
	case ENETRESET:
	case ECONNABORTED:
	case ECONNRESET:
	case ENOBUFS:
	case ENOTCONN:
	case ESHUTDOWN:
	case ECONNREFUSED:
	case EHOSTDOWN:
	case EHOSTUNREACH:

	    result=1;
	    break;

    }

    return result;

}

unsigned char socket_blocking_error(unsigned int error)
{
    unsigned char result=0;

    switch (error) {

	case EWOULDBLOCK:

	    result=1;
	    break;
    }

    return result;

}

#ifdef __linux__

int set_socket_properties(struct osns_socket_s *sock, struct socket_properties_s *prop)
{
    int result=-1;

    if ((sock->type == OSNS_SOCKET_TYPE_CONNECTION) && (sock->flags & OSNS_SOCKET_FLAG_LOCAL)) {

	/* check anything is to be set */
	if ((prop->valid & (SOCKET_PROPERTY_FLAG_OWNER | SOCKET_PROPERTY_FLAG_GROUP))==0) return 0;

	struct fs_location_path_s path;

	if (get_path_osns_sockaddr(sock, &path)>0) {
	    struct system_stat_s stat;
	    unsigned int mask=0;

	    memset(&stat, 0, sizeof(struct system_stat_s));

	    if ((prop->valid & SOCKET_PROPERTY_FLAG_OWNER) && prop->owner) {

		struct passwd *pwd=getpwnam(prop->owner);

		if (pwd) {

		    mask |= SYSTEM_STAT_UID;
		    set_uid_system_stat(&stat, pwd->pw_uid);

		}

	    }

	    if ((prop->valid & SOCKET_PROPERTY_FLAG_GROUP) && prop->group) {

		struct group *grp=getgrnam(prop->group);

		if (grp) {

		    mask |= SYSTEM_STAT_GID;
		    set_gid_system_stat(&stat, grp->gr_gid);

		}

	    }

	    if ((prop->valid & SOCKET_PROPERTY_FLAG_MODE) && prop->mode>0) {

		mask |= SYSTEM_STAT_MODE;
		set_mode_system_stat(&stat, prop->mode);

	    }

	    if (mask>0) {

		if (system_setstat(&path, mask, &stat)==0) {

		    logoutput("set_socket_properties: successfully set");
		    result=(int) mask;

		} else {

		    logoutput("set_socket_properties: not everything set");

		}

	    }

	}

    }

    return result;

}

#else

int set_socket_properties(struct osns_socket_s *sock, struct socket_properties_s *prop)
{
    return -1;
}

#endif

int get_local_peer_properties(struct osns_socket_s *sock, struct local_peer_s *peer)
{
    struct ucred cred;
    struct generic_socket_option_s option;
    int result=-1;

    if ((sock->type != OSNS_SOCKET_TYPE_CONNECTION) || ((sock->flags & OSNS_SOCKET_FLAG_LOCAL)==0)) return -1;

    /* get credentials: peer uid/gid/pid */

    memset(&cred, 0, sizeof(struct ucred));

    option.level=SOL_SOCKET;
    option.type=SO_PEERCRED;
    option.value=(char *) &cred;
    option.len=sizeof(struct ucred);

    if ((* sock->getsockopt)(sock, &option)==0) {

	peer->uid=cred.uid;
	peer->gid=cred.gid;
	peer->pid=cred.pid;
	result=0;

    } else {

	logoutput("get_local_peer_properties: error %i geting socket credentials (%s)", errno, strerror(errno));

    }

    return result;

}

static unsigned int max_length_path_local_socket=(sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path));

int set_path_osns_sockaddr(struct osns_socket_s *sock, struct fs_location_path_s *path)
{

    logoutput_debug("set_path_osns_sockaddr: max %u path %.*s", max_length_path_local_socket, path->len, path->ptr);

    if ((sock->type == OSNS_SOCKET_TYPE_CONNECTION) &&  (sock->flags & OSNS_SOCKET_FLAG_LOCAL)) {
	struct sockaddr_un *sun=&sock->data.connection.sockaddr.data.local;

	memset(sun->sun_path, 0, max_length_path_local_socket);

	if (path->len==0) {

	    return 0;

	} else if (path->len<max_length_path_local_socket) {

	    memcpy(sun->sun_path, path->ptr, path->len);
	    return path->len;

	}

    }

    /* no truncate ... too long path is error
	or type is wrong */
    return -1;
}

unsigned int get_path_osns_sockaddr(struct osns_socket_s *sock, struct fs_location_path_s *path)
{

    path->ptr=NULL;
    path->len=0;

    if ((sock->type == OSNS_SOCKET_TYPE_CONNECTION) && (sock->flags & OSNS_SOCKET_FLAG_LOCAL)) {
	char *sep=NULL;
	struct sockaddr_un *sun=&sock->data.connection.sockaddr.data.local;

	path->ptr=sun->sun_path;
	path->len=max_length_path_local_socket;

	sep=memchr(sun->sun_path, '\0', max_length_path_local_socket);
	if (sep) path->len=(unsigned int)(sep - path->ptr);

    }

    return path->len;

}

int set_address_osns_sockaddr(struct osns_socket_s *sock, struct ip_address_s *address, unsigned int port)
{
    int result=-1;

    if ((sock->type==OSNS_SOCKET_TYPE_CONNECTION) && (sock->flags & OSNS_SOCKET_FLAG_NET)) {

	if (sock->flags & OSNS_SOCKET_FLAG_IPv6) {
	    struct sockaddr_in6 *sin6=NULL;

	    if ((address->family!=IP_ADDRESS_FAMILY_IPv6) || check_family_ip_address(address->addr.v6, "ipv6")==0) goto out;

	    sin6=&sock->data.connection.sockaddr.data.net6;

	    memset(sin6, 0, sizeof(struct sockaddr_in6));
	    sin6->sin6_family=AF_INET6;
	    sin6->sin6_port=htons(port);
	    inet_pton(AF_INET6, address->addr.v6, &sin6->sin6_addr);
	    result=0;

	} else if (sock->flags & OSNS_SOCKET_FLAG_IPv4) {
	    struct sockaddr_in *sin=NULL;

	    if ((address->family!=IP_ADDRESS_FAMILY_IPv4) || check_family_ip_address(address->addr.v4, "ipv4")==0) goto out;

	    sin=&sock->data.connection.sockaddr.data.net4;

	    memset(sin, 0, sizeof(struct sockaddr_in));
	    sin->sin_family=AF_INET;
	    sin->sin_port=htons(port);
	    inet_pton(AF_INET, address->addr.v4, &sin->sin_addr);
	    result=0;

	}

    }

    out:
    return result;
}

static int get_socket_addr_hostname(char *hostname, unsigned int size, struct osns_sockaddr_s *sockaddr)
{
    int result=0;
    unsigned int error=EIO;
    unsigned char count=0;

    gethostname:
    memset(hostname, 0, size);
    result=getnameinfo(sockaddr->addr, sockaddr->len, hostname, size, NULL, 0, NI_NAMEREQD);
    count++;

    switch (result) {

	case 0:

	    return 0;

	case EAI_MEMORY:

	    error=ENOMEM;
	    break;

	case EAI_NONAME:

	    error=ENOENT;
	    break;

	case EAI_SYSTEM:

	    error=errno;
	    break;

	case EAI_OVERFLOW:

	    error=ENAMETOOLONG;
	    break;

	case EAI_AGAIN:

	    if (count<10) goto gethostname; /* try again*/
	    error=EAGAIN;
	    break;

	default:

	    break;

    }

    logoutput_warning("get_socket_addr_hostname: error %i getting hostname (%s)", error, strerror(error));
    return error;

}

int get_network_peer_properties(struct osns_socket_s *sock, struct network_peer_s *peer, const char *what)
{
    int result=-1;
    struct osns_sockaddr_s sockaddr;
    char *buffer=NULL;
    unsigned int size=0;
    int domain=0;

    memset(&sockaddr, 0, sizeof(struct osns_sockaddr_s));

    if (sock->flags & OSNS_SOCKET_FLAG_IPv4) {

	sockaddr.addr=(struct sockaddr *) &sockaddr.data.net4;
	sockaddr.len=sizeof(struct sockaddr_in);
	buffer=peer->host.ip.addr.v4;
	size=INET_ADDRSTRLEN;
	domain=AF_INET;

    } else if (sock->flags & OSNS_SOCKET_FLAG_IPv6) {

	sockaddr.addr=(struct sockaddr *) &sockaddr.data.net6;
	sockaddr.len=sizeof(struct sockaddr_in6);
	buffer=peer->host.ip.addr.v6;
	size=INET6_ADDRSTRLEN;
	domain=AF_INET6;

    }

    if (sockaddr.addr) {

#ifdef __linux__
	int fd=(* sock->get_unix_fd)(sock);

	if (strcmp(what, "remote")==0) {

	    result=getpeername(fd, sockaddr.addr, &sockaddr.len);

	} else if (strcmp(what, "local")==0) {

	    result=getsockname(fd, sockaddr.addr, &sockaddr.len);

	}

	if (result==0) {

	    if (peer->host.flags & HOST_ADDRESS_FLAG_IP) {
		void *src=NULL;

		if (domain==AF_INET6) {
		    struct sockaddr_in6 *tmp=(struct sockaddr_in6 *) sockaddr.addr;

		    src=(void *) &tmp->sin6_addr;

		} else if (domain==AF_INET) {
		    struct sockaddr_in *tmp=(struct sockaddr_in *) sockaddr.addr;

		    src=(void *) &tmp->sin_addr;

		}

		memset(buffer, 0, size);

		if (inet_ntop(domain, src, buffer, size)) {

		    logoutput_debug("get_network_peer_properties: found ip %s", buffer);
		    peer->host.ip.family=domain;

		} else {

		    result=-1;
		    logoutput_debug("get_network_peer_properties: error %u retreiving ip number", result);
		    goto out;

		}

	    }

	    if (peer->host.flags & HOST_ADDRESS_FLAG_HOSTNAME) {

		result=get_socket_addr_hostname(peer->host.hostname, HOST_HOSTNAME_FQDN_MAX_LENGTH, &sockaddr);

		if (result==0) {

		    logoutput_debug("get_network_peer_properties: found hostname %s", peer->host.hostname);

		} else {

		    logoutput_debug("get_network_peer_properties: error %u retreiving hostname");
		    result=-1;

		}

	    }

	} else if (result==-1) {

	    logoutput_debug("get_network_peer_properties: error %u:%s getting %s from fd %u", errno, strerror(errno), fd);

	}

#endif

    }

    out:
    return result;
}

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

static unsigned int typeflags=(OSNS_SOCKET_FLAG_CHAR_DEVICE | OSNS_SOCKET_FLAG_BLOCK_DEVICE);

static char *get_name_system_device(unsigned int flags)
{
    char *name="unknown";

    if (flags & OSNS_SOCKET_FLAG_CHAR_DEVICE) {

	name="char device";

    } else if (flags & OSNS_SOCKET_FLAG_BLOCK_DEVICE) {

	name="block device";

    }

    return name;
}

static int socket_open_dummy(struct osns_socket_s *sock, struct fs_path_s *path)
{
    return -1;
}

static void set_device_socket_cb(struct osns_socket_s *sock, unsigned char enable);

static int socket_open(struct osns_socket_s *sock, struct fs_path_s *path)
{
    struct system_stat_s stat;
    int result=-1;

    /* a connection to a local character or block device,
	a valid path to device is required */

    if ((path==NULL) || (fs_path_get_length(path)==0)) return -1;

    /* device/file must exist */

    if (system_getstat(path, SYSTEM_STAT_TYPE, &stat)==0) {
	unsigned int typeflag = (sock->flags & typeflags);
	unsigned int tmp=0;

	if (system_stat_test_ISCHR(&stat)) {

	    tmp = OSNS_SOCKET_FLAG_CHAR_DEVICE;

	} else if (system_stat_test_ISBLK(&stat)) {

	    tmp = OSNS_SOCKET_FLAG_BLOCK_DEVICE;

	}

	if (tmp==0) {

	    logoutput_debug("socket_open: fs object not reckognized");
	    return -1;

	} else if (typeflag != tmp) {

	    logoutput_debug("socket_open: fs object wrong specified (as %u, found %u)", typeflag, tmp);
	    return -1;

	} else {
	    unsigned int size=fs_path_get_length(path);
	    char buffer[size+1];
	    unsigned int openflags=translate_osns_socket_flags(sock->flags);
	    int fd=0;

	    memset(buffer, 0, size+1);
	    size=fs_path_copy(path, buffer, size);

#ifdef __linux__

	    fd=open(buffer, openflags);

	    if (fd>=0) {

		set_device_socket_cb(sock, 1);
		sock->status |= SOCKET_STATUS_OPEN;
		result=0;
		logoutput_debug("socket_open: open %s %s with fd %i flags %u", get_name_system_device(tmp), buffer, fd, openflags);
		(* sock->set_unix_fd)(sock, fd);

	    } else {

		logoutput_debug("socket_open: unable to open %s", buffer);

	    }

#endif


	}

    }

    return result;

}

static void set_device_socket_cb(struct osns_socket_s *sock, unsigned char enable)
{

    sock->sops.device.open=(enable ? socket_open_dummy : socket_open);

    sock->sops.device.read=(enable ? socket_read_common : socket_readwrite_dummy);
    sock->sops.device.write=(enable ? socket_write_common : socket_readwrite_dummy);
    sock->sops.device.writev=(enable ? socket_writev_common : socket_writevreadv_dummy);
    sock->sops.device.readv=(enable ? socket_readv_common : socket_writevreadv_dummy);

}

static void cb_change_device(struct osns_socket_s *sock, unsigned int what)
{

    if (what==SOCKET_CHANGE_OP_SET) {

	int fd=(* sock->get_unix_fd)(sock);

	if (fd>=0) set_device_socket_cb(sock, 1);

    }

}

void init_osns_device_socket(struct osns_socket_s *sock)
{

    logoutput_debug("init_osns_device_socket");
    set_device_socket_cb(sock, 0);
    sock->change=cb_change_device;

}

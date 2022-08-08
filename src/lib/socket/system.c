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

static unsigned int atypes=(SYSTEM_SOCKET_TYPE_CHAR_DEVICE | SYSTEM_SOCKET_TYPE_BLOCK_DEVICE | SYSTEM_SOCKET_TYPE_SYSTEM_FILE);

static char *get_name_system_device(unsigned int type)
{
    char *name="unknown";

    if (type==SYSTEM_SOCKET_TYPE_CHAR_DEVICE) {

	name="char device";

    } else if (type==SYSTEM_SOCKET_TYPE_BLOCK_DEVICE) {

	name="block device";

    } else if (type==SYSTEM_SOCKET_TYPE_SYSTEM_FILE) {

	name="system file";

    }

    return name;
}

static void set_system_socket_sops(struct system_socket_s *sock)
{
    sock->sops.type.system.read=socket_read_common;
    sock->sops.type.system.write=socket_write_common;
    sock->sops.type.system.writev=socket_writev_common;
    sock->sops.type.system.readv=socket_readv_common;
}

void init_system_socket_system(struct system_socket_s *sock, struct fs_location_path_s *path)
{

    logoutput_debug("init_system_socket_system");

    sock->sops.type.system.read=socket_readwrite_dummy;
    sock->sops.type.system.write=socket_readwrite_dummy;
    sock->sops.type.system.readv=socket_writevreadv_dummy;
    sock->sops.type.system.writev=socket_writevreadv_dummy;

    if (sock->flags & SYSTEM_SOCKET_FLAG_NOOPEN) {

	set_system_socket_sops(sock);
	return;

    }

    if ((sock->type & SYSTEM_SOCKET_TYPE_SYSTEM_DEVICE)==0) {
	struct system_stat_s stat;

	/* a connection to a local character or block device,
	    a valid path to device is required */

	if ((path==NULL) || (get_unix_location_path_length(path)==0) || (test_location_path_absolute(path)==0)) return;

	logoutput_debug("init_system_socket_system: path size %u len %u addr %lu", path->size, path->len, (uint64_t) path);

	/* device/file must exist */

	if (system_getstat(path, SYSTEM_STAT_TYPE, &stat)==0) {
	    unsigned int atype = (sock->type & atypes);
	    unsigned int tmp=0;

	    if (system_stat_test_ISCHR(&stat)) {

		tmp = SYSTEM_SOCKET_TYPE_CHAR_DEVICE;

	    } else if (system_stat_test_ISBLK(&stat)) {

		tmp = SYSTEM_SOCKET_TYPE_BLOCK_DEVICE;

	    } else if (system_stat_test_ISREG(&stat)) {

		tmp = SYSTEM_SOCKET_TYPE_SYSTEM_FILE;

	    }

	    if (tmp==0) {

		logoutput_debug("init_system_socket_system: path to object not reckognized");
		return;

	    } else if (atype>0 && (atype != tmp)) {

		logoutput_debug("init_system_socket_system: fs object wrong specified (as %u, found %u)", tmp, atype);
		return;

	    } else {
		unsigned int size=get_unix_location_path_length(path);
		char buffer[size+1];
		unsigned int openflags=translate_system_socket_flags(sock->flags);

		if (atype==0) sock->type |= tmp;

		memset(buffer, 0, size+1);
		size=copy_unix_location_path(path, buffer, size);

#ifdef __linux__

		sock->backend.fd=open(buffer, (openflags));

		if (sock->backend.fd>=0) {

		    sock->status |= SOCKET_STATUS_OPEN;
		    set_system_socket_sops(sock);
		    logoutput_debug("init_system_socket_system: open %s %s with fd %i flags %u", get_name_system_device(tmp), buffer, sock->backend.fd, openflags);

		} else {

		    logoutput_debug("init_system_socket_system: unable to open %s", buffer);

		}

#endif

	    }

	}

    }

}

void set_unix_fd_system(struct system_socket_s *sock)
{
    set_system_socket_sops(sock);
}

int socket_read(struct system_socket_s *sock, char *buffer, unsigned int size)
{
    return (* sock->sops.type.system.read)(sock, buffer, size);
}

int socket_write(struct system_socket_s *sock, char *buffer, unsigned int size)
{
    return (* sock->sops.type.system.write)(sock, buffer, size);
}

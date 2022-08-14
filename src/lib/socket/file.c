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

static int socket_preadwrite_dummy(struct osns_socket_s *s, char *data, unsigned int size, off_t off)
{
    return -1;
}

static int socket_pread_common(struct osns_socket_s *s, char *data, unsigned int size, off_t off)
{

#ifdef __linux__

    return pread(s->fd, data, size, off);

#else

    return -1;

#endif

}

static int socket_pwrite_common(struct osns_socket_s *s, char *data, unsigned int size, off_t off)
{

#ifdef __linux__

    return pwrite(s->fd, data, size, off);

#else

    return -1;

#endif
}

static int socket_fsyncflush_dummy(struct osns_socket_s *s, unsigned int flags)
{
    return -1;
}

static int socket_fsync_common(struct osns_socket_s *s, unsigned int flags)
{

#ifdef __linux__

    return ((flags & OSNS_SOCKET_FSYNC_FLAG_DATA) ? fdatasync(s->fd) : fsync(s->fd));

#else

    return -1;

#endif

}

int socket_flush_common(struct osns_socket_s *socket, unsigned int flags)
{

    /* what to do here ?? */
    return 0;
}

off_t socket_lseek_dummy(struct osns_socket_s *s, off_t off, int whence)
{
    return -1;
}

off_t socket_lseek_common(struct osns_socket_s *s, off_t off, int whence)
{

#ifdef __linux__

    return lseek(s->fd, off, whence);

#else

    return -1;

#endif

}

int socket_fgetstat_dummy(struct osns_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    return -1;
}

int socket_fgetstat_common(struct osns_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    return system_fgetstat(socket, mask, stat);
}

int socket_fsetstat_common(struct osns_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    return system_fsetstat(socket, mask, stat);
}

static int socket_open_dummy(struct osns_socket_s *sock, struct fs_location_path_s *path, struct fs_init_s *init, unsigned int flags)
{
    return -1;
}

static void set_file_socket_cb(struct osns_socket_s *sock, unsigned char enable);

static int socket_open(struct osns_socket_s *ref, struct fs_location_path_s *path, struct osns_socket_s *sock, struct fs_init_s *init, unsigned int flags)
{
    int result=-1;

    /* a connection to a local character or block device,
	a valid path to device is required */

    if ((path==NULL) || (get_unix_location_path_length(path)==0)) {

	return -1;

    } else {
	unsigned int size=get_unix_location_path_length(path);
	char buffer[size+1];
	unsigned int openflags=translate_osns_socket_flags(sock->flags);
	int fd=-1;

	/* device/file must exist */

	memset(buffer, 0, size+1);
	size=copy_unix_location_path(path, buffer, size);

#ifdef __linux__

	if (buffer[0]=='/') {

	    if (openflags & O_CREAT) {

		if (init==NULL) return -1;
		fd=open(buffer, openflags, init->mode);

	    } else {

		fd=open(buffer, openflags);

	    }

	} else {
	    int dirfd=((ref) ? (* ref->get_unix_fd)(ref) : -1);

	    if (openflags & O_CREAT) {

		if (init==NULL) return -1;
		fd=openat(dirfd, buffer, openflags, init->mode);

	    } else {

		fd=openat(dirfd, buffer, openflags);

	    }

	}

	if (fd>=0) {

	    (* sock->set_unix_fd)(sock, fd);
	    set_file_socket_cb(sock, 1);
	    sock->status |= SOCKET_STATUS_OPEN;
	    result=0;
	    logoutput_debug("socket_open_file: open %s with fd %i flags %u", buffer, fd, openflags);

	} else {

	    logoutput_debug("socket_open_file: unable to open %s", buffer);

	}

#endif

    }

    return result;

}

static void set_file_socket_cb(struct osns_socket_s *sock, unsigned char enable)
{

    sock->sops.filesystem.file.open=(enable ? socket_open_dummy : socket_open);

    sock->sops.filesystem.file.pread=(enable ? socket_read_common : socket_readwrite_dummy);
    sock->sops.filesystem.file.pwrite=(enable ? socket_write_common : socket_readwrite_dummy);
    sock->sops.filesystem.file.fsync=(enable ? socket_fsync_common : socket_fsyncflush_dummy);
    sock->sops.filesystem.file.flush=(enable ? socket_flush_common : socket_fsyncflush_dummy);
    sock->sops.filesystem.file.lseek=(enable ? socket_lseek_common : socket_lseek_dummy);
    sock->sops.filesystem.file.fgetstat=(enable ? socket_fgetstat_common : socket_fgetstat_dummy);
    sock->sops.filesystem.file.fsetstat=(enable ? socket_fsetstat_common : socket_fgetstat_dummy);

}

void init_osns_file_socket(struct osns_socket_s *sock)
{

    logoutput_debug("init_osns_file_socket");
    set_file_socket_cb(sock, 0);

}

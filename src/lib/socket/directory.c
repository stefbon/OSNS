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
#include <dirent.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "lib/system/stat.h"

#include "socket.h"
#include "common.h"
#include "utils.h"

#ifdef __linux__

struct linux_dirent64 {
    ino64_t			d_ino;
    off64_t			d_off;
    unsigned short		d_reclen;
    unsigned char		d_type;
    char			d_name[];
};

static struct fs_dentry_s *get_dentry_dummy(struct osns_socket_s *socket, unsigned char next)
{
    return NULL;
}

static struct fs_dentry_s *get_dentry(struct osns_socket_s *socket, unsigned char next)
{
    struct linux_dirent64 *dirent=NULL;
    struct fs_dentry_s *dentry=&socket->data.directory.dentry;
    char *pos=NULL;
    unsigned int len=0;

    if (next==0) return dentry;

    readdentry:

    if (socket->data.directory.pos >= socket->data.directory.read) {
	int result=-1;
	int fd=(* socket->get_unix_fd)(socket);

	/* read a batch of dirents */

	result=syscall(SYS_getdents64, fd, (struct linux_dirent64 *) socket->data.directory.buffer, socket->data.directory.size);

	if (result>0) {

	    /* read successfull */
	    socket->data.directory.pos=0;
	    socket->data.directory.read=(unsigned int) result;

	    /* clear any previous dentry found */

	    dentry->flags=0;
	    dentry->type=0;
	    dentry->ino=0;
	    dentry->len=0;
	    dentry->name=NULL;

	} else if (result==0) {

	    /* see man getdents: 0 means End Of Data */
	    socket->data.directory.flags |= OSNS_SOCKET_DIRECTORY_FLAG_EOD;
	    return NULL;

	} else {

	    socket->data.directory.flags |= OSNS_SOCKET_DIRECTORY_FLAG_ERROR;
	    return NULL;

	}

    }

    dirent=(struct linux_dirent64 *) (socket->data.directory.buffer + socket->data.directory.pos);
    socket->data.directory.pos += dirent->d_reclen;

    /* TODO: a filter based on name */

    dentry->type=DTTOIF(dirent->d_type);
    dentry->ino=dirent->d_ino;
    dentry->name=dirent->d_name;
    dentry->len=dirent->d_reclen - offsetof(struct linux_dirent64, d_name) - 1; /* minus the terminating byte */
    /* use rawmemchr ?? dangerous .... no check if there is no zero byte found */
    // pos=memchr(dentry->name, '\0', len);
    // dentry->len=(pos) ? (unsigned int)(pos - dentry->name) : len;
    return dentry;

}

#endif

int socket_fstatat_dummy(struct osns_socket_s *socket, char *name, unsigned int mask, struct system_stat_s *stat, unsigned int flags)
{
    return -1;
}

int socket_fstatat(struct osns_socket_s *socket, char *name, unsigned int mask, struct system_stat_s *stat, unsigned int flags)
{
    return system_fgetstatat(socket, name, mask, stat, flags);
}

int socket_unlinkat_dummy(struct osns_socket_s *socket, const char *name, unsigned int flags)
{
    return -1;
}

int socket_unlinkat(struct osns_socket_s *socket, const char *name, unsigned int flags)
{

#ifdef __linux__

    int fd=(* socket->get_unix_fd)(socket);
    return unlinkat(fd, name, flags);

#else

    return -1;

#endif

}

int socket_rmdirat(struct osns_socket_s *socket, const char *name, unsigned int flags)
{

#ifdef __linux__

    int fd=(* socket->get_unix_fd)(socket);
    flags |= AT_REMOVEDIR;
    return unlinkat(fd, name, flags);

#else

    return -1;

#endif

}

int socket_fsyncdir_dummy(struct osns_socket_s *socket, unsigned int flags)
{
    return -1;
}

int socket_fsyncdir(struct osns_socket_s *socket, unsigned int flags)
{

#ifdef __linux__

    int fd=(* socket->get_unix_fd)(socket);
    return ((flags & OSNS_SOCKET_FSYNC_FLAG_DATA) ? fdatasync(fd) : fsync(fd));

#else

    return -1;

#endif

}

int socket_readlinkat_dummy(struct osns_socket_s *socket, const char *name, struct fs_location_path_s *target)
{
    return -1;
}

int socket_readlinkat(struct osns_socket_s *socket, const char *name, struct fs_location_path_s *target)
{

#ifdef __linux__

    unsigned int size=512;
    int fd=(socket ? (* socket->get_unix_fd)(socket) : -1);
    int len=-1;

    trysize:

    if (target->size < size) {

	target->ptr=realloc(target->ptr, size);
	if (target->ptr==NULL) return -ENOMEM;

    }

    memset(target->ptr, 0, size);
    target->len=0;
    target->size=size;
    target->flags|=FS_LOCATION_PATH_FLAG_PTR_ALLOC;

    len=readlinkat(fd, name, target->ptr, size);

    if (len==-1) {

	/* free the allocated buffer in target->ptr ? */
	return -errno;

    } else if (len<size) {

	target->len=len;
	target->ptr[len]='\0';
	return len;

    }

    size+=512;
    if (size<=PATH_MAX) goto trysize;
    return -ENAMETOOLONG;

#else

    return -1;

#endif

}

static int socket_open_dummy(struct osns_socket_s *ref, struct fs_location_path_s *path, struct osns_socket_s *sock, struct fs_init_s *init, unsigned int flags)
{
    return -1;
}

static void set_directory_socket_cb(struct osns_socket_s *sock, unsigned char enable);

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

	memset(buffer, 0, size+1);
	size=copy_unix_location_path(path, buffer, size);

#ifdef __linux__

	openflags |= O_DIRECTORY;

	if (buffer[0]=='/') {

	    /* absolute path -> ignore the ref socket */

	    fd=open(buffer, openflags);

	} else {
	    int dirfd=((ref) ? (* ref->get_unix_fd)(ref) : -1);

	    if (dirfd>=0) fd=openat(dirfd, buffer, openflags);

	}

	if (fd>=0) {

	    (* sock->set_unix_fd)(sock, fd);
	    set_directory_socket_cb(sock, 1);
	    sock->status |= SOCKET_STATUS_OPEN;
	    result=0;
	    logoutput_debug("socket_open_directory: open %s with fd %i flags %u", buffer, fd, openflags);

	} else {

	    logoutput_debug("socket_open_directory: unable to open %s", buffer);

	}

#endif

    }

    return result;

}

static void set_directory_socket_cb(struct osns_socket_s *sock, unsigned char enable)
{

    sock->sops.filesystem.dir.open=(enable ? socket_open_dummy : socket_open);

    sock->sops.filesystem.dir.get_dentry=(enable ? get_dentry : get_dentry_dummy);
    sock->sops.filesystem.dir.fstatat=(enable ? socket_fstatat : socket_fstatat_dummy);
    sock->sops.filesystem.dir.unlinkat=(enable ? socket_unlinkat : socket_unlinkat_dummy);
    sock->sops.filesystem.dir.rmdirat=(enable ? socket_rmdirat : socket_unlinkat_dummy);
    sock->sops.filesystem.dir.fsyncdir=(enable ? socket_fsyncdir : socket_fsyncdir_dummy);
    sock->sops.filesystem.dir.readlinkat=(enable ? socket_readlinkat : socket_readlinkat_dummy);

}

void init_osns_directory_socket(struct osns_socket_s *sock)
{

    logoutput_debug("init_osns_directory_socket");
    set_directory_socket_cb(sock, 0);

}

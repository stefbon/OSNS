/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

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

#include <sys/stat.h>
#include <sys/mount.h>
#include <pwd.h>

#include "libosns-log.h"
#include "libosns-list.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"
#include "lib/system/path.h"
#include "libosns-socket.h"

static int get_fuse_mountoptions(char *buffer, unsigned int size, struct osns_socket_s *sock, mode_t mode, uid_t uid, gid_t gid, unsigned int maxread)
{

#ifdef __linux__

    /* get the mountoptions for a FUSE mount,
	note that the default_permissions is not set, which means permissions checcking is done through the access call */

    int fd=(* sock->get_unix_fd)(sock);
    return snprintf(buffer, size, "fd=%i,rootmode=%o,user_id=%i,group_id=%i,max_read=%i", fd, mode, uid, gid, maxread);

#else

    return -1;

#endif

}

static int set_system_fuse_device(struct fs_location_path_s *path)
{

#ifdef __linux__

    set_location_path(path, 'c', "/dev/fuse");
    return path->len;

#else

    return -1;

#endif

}

int open_fusesocket(struct osns_socket_s *sock)
{
    struct fs_location_path_s fuse_device=FS_LOCATION_PATH_INIT;
    int result=-1;

    if (set_system_fuse_device(&fuse_device)>0) {

	init_osns_socket(sock, OSNS_SOCKET_TYPE_DEVICE, (OSNS_SOCKET_FLAG_CHAR_DEVICE | OSNS_SOCKET_FLAG_RDWR));
	result=(* sock->sops.device.open)(sock, &fuse_device);

    }

    return result;
}

/* connect the fuse interface with the target: the VFS/kernel */

#ifdef __linux__

static int create_mountpoint(char *mountpoint)
{
    int result=-1;
    unsigned int len=strlen(mountpoint);
    char *pos=mountpoint;
    char *sep=NULL;
    int left=(int) len;

    dostep:

    sep=memchr(pos, '/', left);
    if (sep) {

	if (sep==pos) {

	    if (left==1) goto out;
	    pos++;
	    left--;
	    goto dostep;

	}

	*sep='\0';

    }

    result=access(mountpoint, F_OK | R_OK | W_OK | X_OK);

    if (result==-1) {

	if (errno==ENOENT) {

	    result=mkdir(mountpoint, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

	}

	if (result==-1) {

	    logoutput("create_mountpoint: error %i:%s creating path %s", errno, strerror(errno), mountpoint);

	}

    }

    if (sep) {

	*sep='/';
	pos=sep+1;
	left=(int)(mountpoint + len - pos);
	if (result==0 && left>0) goto dostep;

    }

    out:
    return result;

}

int mount_fusesocket(struct fs_location_path_s *path, struct osns_socket_s *sock, uid_t uid, gid_t gid, const char *source, const char *fstype, unsigned int maxread, int different_namespace)
{
    mode_t rootmode=(S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    char mountoptions[128];
    unsigned int mountflags=MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_NOATIME;
    int result=-1;

    if ((sock->status & SOCKET_STATUS_OPEN)==0) return -1;

    memset(mountoptions, 0, 128);

    if (get_fuse_mountoptions(mountoptions, 128, sock, rootmode, uid, gid, maxread)>0) {
	unsigned int size=get_unix_location_path_length(path);
	char mountpoint[size + 1];

	memset(mountpoint, 0, size+1);
	copy_unix_location_path(path, mountpoint, size+1);
	logoutput("mount_fusesocket: mounting %s:%s at %s with options %s", source, fstype, mountpoint, mountoptions);
	create_mountpoint(mountpoint);

	if (mount(source, mountpoint, fstype, mountflags, (const void *) mountoptions)==0) {

	    logoutput("mount_fusesocket: mounted %s", mountpoint);
	    result=0;

	    if (different_namespace==1) {

		/* 20220401: TODO mark this mount as private to the namespace */

	    }

	} else {

	    logoutput("mount_fusesocket: error %i:%s mounting %s", errno, strerror(errno), mountpoint);

	}

    }

    return result;

}

void umount_path(struct fs_location_path_s *path)
{

    if (path->len>0) {
	unsigned int size=get_unix_location_path_length(path);
	char mountpoint[size + 1];

	memset(mountpoint, 0, size+1);
	copy_unix_location_path(path, mountpoint, size);

	logoutput_debug("umount_path: path %s", mountpoint);
	umount2(mountpoint, MNT_DETACH);

    }

}

#else

int mount_fusesocket(struct fs_location_path_s *path, struct osns_socket_s *sock, uid_t uid, gid_t gid, char *source, char *fstype, unsigned int maxread, int different_namespace)
{
    return -1;
}

void umount_path(struct fs_location_path_s *path)
{}

#endif

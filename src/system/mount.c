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

#include <sys/mount.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <pwd.h>

#include "libosns-log.h"
#include "libosns-list.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"

#include "osns-protocol.h"

#include "lib/system/path.h"
#include "libosns-socket.h"
#include "lib/fuse/mount.h"

#include "receive.h"
#include "mount.h"
#include "fuse.h"

#define MOUNTLIST_FLAG_LOCK			1
static char fuse_receive_buffer[8192];

/* 20220417: more fields here ? default flags and options ... ?? */

struct mount_location_s {
    const char					*base;
    unsigned char				type;
    const char					*source;
    const char					*fstype;
};

static struct mount_location_s mount_locations[] = {
    {	.base			= OSNS_DEFAULT_NETWORK_MOUNT_PATH,
	.type			= OSNS_MOUNT_TYPE_NETWORK,
	.source			= "network@osns.net",
	.fstype			= "fuse.network",
    },
    {	.base			= OSNS_DEFAULT_DEVICES_MOUNT_PATH,
	.type			= OSNS_MOUNT_TYPE_DEVICES,
	.source			= "devices@osns.net",
	.fstype			= "fuse.devices",
    },
    {	.base			= NULL,
	.type			= 0,
	.source			= NULL,
	.fstype			= NULL,
    },
};

static unsigned int get_mount_location(struct fs_location_path_s *path, unsigned char type, unsigned int uid, const char **source, const char **fstype)
{
    unsigned int size=0;
    unsigned int (* append_cb)(struct fs_location_path_s *path, const unsigned char type, void *ptr);
    unsigned int index=0;

    if (path->ptr) {

	append_cb=append_location_path;

    } else {

	append_cb=append_location_path_get_required_size;

    }

    while (mount_locations[index].base) {

	if (type==mount_locations[index].type) {

	    logoutput_debug("get_mount_location: base path %s", mount_locations[index].base);

	    size 	=	(* append_cb)(path, 'c', (void *) mount_locations[index].base);
	    if (source) *source=mount_locations[index].source;
	    if (fstype) *fstype=mount_locations[index].fstype;
	    break;

	}

	index++;

    }

    if (size>0) {

	// size 	+=	(* append_cb)(path, 'c', (void *) "/");
	size 	+=	(* append_cb)(path, 'u', (void *) &uid);
	size 	+=	(* append_cb)(path, 'c', (void *) "/fs");
	size	+=	8;

    } else {

	logoutput_warning("get_mount_location: type %u not supported", (unsigned int) type);

    }

    return size;

}

static struct osns_mount_s *find_osns_mount(struct osns_systemconnection_s *sc, unsigned char type)
{
    struct osns_mount_s *osns_mount=NULL;
    struct list_element_s *list=NULL;

    list=get_list_head(&sc->mounts, 0);

    while (list) {

	osns_mount=(struct osns_mount_s *)((char *) list - offsetof(struct osns_mount_s, list));
	if (osns_mount->type==type) break;
	list=get_next_element(list);
	osns_mount=NULL;

    }

    return osns_mount;
}

static void add_osns_mount(struct osns_systemconnection_s *sc, struct osns_mount_s *om)
{
    add_list_element_last(&sc->mounts, &om->list);
}

static int test_difference_namespace(pid_t a, pid_t b)
{

    /* 20220401: for now a dummy, TO BE DONE
    under Linux for OSNS compare the symlinks of /proc/%pid%/ns/mnt and of /proc/%pid%/ns/user between the two pids
    for osns now only these two matter
    */

    return 0;
}

unsigned char test_mountpoint_is_osns_filesystem(struct fs_location_path_s *path)
{
    unsigned int index=0;
    unsigned char result=0;

    while (mount_locations[index].base) {

	result=test_location_path_subdirectory(path, 'c', (void *) mount_locations[index].base, NULL);
	if (result==1) break;
	index++;

    }

    return result;

}

static int notify_VFS_system_cb(struct fuse_receive_s *r, unsigned int code, struct iovec *iov, unsigned int count)
{
    struct osns_mount_s *om=(struct osns_mount_s *)((char *) r - offsetof(struct osns_mount_s, receive));
    struct osns_socket_s *sock=&om->sock;

    return fuse_socket_notify(sock, code, iov, count);
}

static int reply_VFS_system_cb(struct fuse_receive_s *r, uint64_t unique, char *data, unsigned int size)
{
    struct osns_mount_s *om=(struct osns_mount_s *)((char *) r - offsetof(struct osns_mount_s, receive));
    struct osns_socket_s *sock=&om->sock;

    return fuse_socket_reply_data(sock, unique, data, size);
}

static int error_VFS_system_cb(struct fuse_receive_s *r, uint64_t unique, unsigned int code)
{
    struct osns_mount_s *om=(struct osns_mount_s *)((char *) r - offsetof(struct osns_mount_s, receive));
    struct osns_socket_s *sock=&om->sock;

    return fuse_socket_reply_error(sock, unique, code);
}

/* connect the fuse interface with the target: the VFS/kernel */

struct osns_mount_s *mount_fuse_filesystem(struct osns_systemconnection_s *sc, struct shared_signal_s *signal, unsigned char type, unsigned int maxread, unsigned int *p_status)
{
    struct osns_mount_s *om=NULL;
    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
    unsigned int size=get_mount_location(&path, type, sc->connection.ops.client.peer.local.uid, NULL, NULL);
    char buffer[size+1];
    int different_namespace=-1;
    const char *source=NULL;
    const char *fstype=NULL;
    int result=-1;

    if (size==0) {

	logoutput_warning("mount_fuse_filesystem: invalid location/type");
	if (p_status) *p_status=OSNS_STATUS_INVALIDFLAGS;
	return NULL;

    }

    memset(buffer, 0, size+1);
    assign_buffer_location_path(&path, buffer, size);
    size=get_mount_location(&path, type, sc->connection.ops.client.peer.local.uid, &source, &fstype);

    signal_lock_flag(signal, &sc->mounts.flags, MOUNTLIST_FLAG_LOCK);

    om=find_osns_mount(sc, type);

    if (om) {

	if (om->status & OSNS_MOUNT_STATUS_MOUNTED) {

	    logoutput_warning("mount_fuse_filesystem: type %u already mounted for %u", type, sc->connection.ops.client.peer.local.uid);
	    if (p_status) *p_status=OSNS_STATUS_ALREADYMOUNTED;
	    om=NULL;
	    goto unlockmounts;

	}

    }

    if (om==NULL) {

	om=malloc(sizeof(struct osns_mount_s));
	if (om==NULL) {

	    logoutput_warning("mount_fuse_filesystem: not able to allocate osns mount");
	    if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
	    goto unlockmounts;

	}

	memset(om, 0, sizeof(struct osns_mount_s));
	om->status=OSNS_MOUNT_STATUS_INIT;
	om->type=type;
	om->signal=signal;
	init_list_element(&om->list, NULL);

	om->receive.status=0;
	om->receive.flags=0;
	om->receive.ptr=NULL;
	om->receive.loop=get_default_mainloop();

	om->receive.process_data=osns_system_process_fuse_data;
	om->receive.close_cb=osns_system_process_fuse_close;
	om->receive.error_cb=osns_system_process_fuse_close;

	om->receive.notify_VFS=notify_VFS_system_cb;
	om->receive.reply_VFS=reply_VFS_system_cb;
	om->receive.error_VFS=error_VFS_system_cb;

	om->receive.read=0;
	om->receive.size=8192;
	om->receive.threads=0;
	om->receive.buffer=fuse_receive_buffer;

    }

    /* here test the process this mount is done for is in the same namespace or not
	if not then make this thread join that namespace temporarly to do the actual mount */

    different_namespace=test_difference_namespace(getpid(), sc->connection.ops.client.peer.local.pid);

    if (different_namespace==1) {

	/* TODO: enter clients namespace via pidfd_open and setns
	    keep the current namespace somewhere to return later to */

    }

    if ((om->status & OSNS_MOUNT_STATUS_OPEN)==0) {

	if (open_fusesocket(&om->sock)==0) {

	    logoutput_debug("mount_fuse_filesystem: open fusesocket success");
	    om->status |= OSNS_MOUNT_STATUS_OPEN;

	} else {

	    logoutput_debug("mount_fuse_filesystem: failed to open fusesocket");
	    free(om);
	    if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
	    goto exitnamespace;

	}

    }

    if (mount_fusesocket(&path, &om->sock, sc->connection.ops.client.peer.local.uid, sc->connection.ops.client.peer.local.gid, source, fstype, maxread, different_namespace)==0) {

	logoutput_debug("mount_fuse_filesystem: %s mounted at %s", source, buffer);
	add_osns_mount(sc, om);
	om->status |= OSNS_MOUNT_STATUS_MOUNTED;
	result=0;

    } else {

	logoutput_debug("mount_fuse_filesystem: failed to mount %s at %s", source, buffer);
	(* om->sock.sops.close)(&om->sock);
	if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
	free(om);
	om=NULL;

    }

    exitnamespace:

    if (different_namespace==1) {

	/* TODO: leave the clients namespace ... in practice return to orginal namespace */

    }

    unlockmounts:
    signal_unlock_flag(signal, &sc->mounts.flags, MOUNTLIST_FLAG_LOCK);
    return om;

}

void umount_one_fuse_fs(struct osns_systemconnection_s *sc, struct osns_mount_s *om)
{
    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
    struct osns_socket_s *sock=&om->sock;
    unsigned int size=0;

    if ((om->status & OSNS_MOUNT_STATUS_MOUNTED)==0) return;

    if (sc==NULL) {
	struct list_header_s *h=om->list.h;

	if (h) {

	    sc=(struct osns_systemconnection_s *)((char *) h - offsetof(struct osns_systemconnection_s, mounts));

	} else {

	    logoutput_warning("umount_one_fuse_fs: cannot find osns systemdetection ... unable to umount");
	    return;

	}

    }

    size=get_mount_location(&path, om->type, sc->connection.ops.client.peer.local.uid, NULL, NULL);

    if (size>0) {
	char buffer[size+1];

	/* create the mountpath again */

	memset(buffer, 0, size+1);
	assign_buffer_location_path(&path, buffer, size+1);
	size=get_mount_location(&path, om->type, sc->connection.ops.client.peer.local.uid, NULL, NULL);

	logoutput_debug("umount_one_fuse_fs: %s", buffer);
	umount_path(&path);
	om->status &= ~OSNS_MOUNT_STATUS_MOUNTED;

    }

    (* sock->sops.close)(sock);

}

int umount_fuse_filesystem(struct osns_systemconnection_s *sc, struct shared_signal_s *signal, unsigned char type)
{
    struct osns_mount_s *om=NULL;
    int result=0;

    signal_lock_flag(signal, &sc->mounts.flags, MOUNTLIST_FLAG_LOCK);

    om=find_osns_mount(sc, type);

    if (om) {

	umount_one_fuse_fs(sc, om);
	remove_list_element(&om->list);
	free(om);
	result=1;

    }

    signal_unlock_flag(signal, &sc->mounts.flags, MOUNTLIST_FLAG_LOCK);
    return result;
}

void umount_all_fuse_filesystems(struct osns_systemconnection_s *sc, struct shared_signal_s *signal)
{
    struct list_element_s *list=NULL;

    logoutput_debug("umount_all_fuse_filesystems");

    signal_lock_flag(signal, &sc->mounts.flags, MOUNTLIST_FLAG_LOCK);

    list=get_list_head(&sc->mounts, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct osns_mount_s *om=(struct osns_mount_s *)((char *) list - offsetof(struct osns_mount_s, list));

	umount_one_fuse_fs(sc, om);
	remove_list_element(&om->list);
	free(om);

	list=get_list_head(&sc->mounts, SIMPLE_LIST_FLAG_REMOVE);

    }

    signal_unlock_flag(signal, &sc->mounts.flags, MOUNTLIST_FLAG_LOCK);
}

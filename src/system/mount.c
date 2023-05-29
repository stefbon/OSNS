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

#include "system.h"
#include "mount.h"
#include "fuse.h"
#include "osns/send/reply.h"

#define MOUNTLIST_FLAG_LOCK			1

#define SYSTEM_FUSE_RECV_BUFFER_SIZE            8192

extern struct connection_s osns_server;
extern struct connection_s *get_osns_client_connection(struct connection_s *s, uid_t uid);

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

static unsigned char find_valid_mount_location_type(unsigned char type)
{
    unsigned int index=0;

    while (mount_locations[index].base) {

	if (mount_locations[index].type==type) return 1;
	index++;

    }

    return 0;

}

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

	    /* logoutput_debug("get_mount_location: base path %s", mount_locations[index].base); */

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

static struct osns_mount_s *find_osns_mount(struct osns_connection_s *oc, unsigned char type)
{
    struct osns_mount_s *om=NULL;
    struct list_element_s *list=NULL;

    // if ((oc->flags & OSNS_CONNECTION_FLAG_CLIENT)==0) return NULL; /* only a client can ask for mounts */

    list=get_list_head(&oc->type.system2client.mounts);

    while (list) {

	om=(struct osns_mount_s *)((char *) list - offsetof(struct osns_mount_s, list));
	logoutput_debug("find_osns_mount: found mount type %u looking for %u", om->type, type);
	if (om->type==type) break;
	list=get_next_element(list);
	om=NULL;

    }

    return om;
}

static void add_osns_mount(struct osns_connection_s *oc, struct osns_mount_s *om)
{
    add_list_element_last(&oc->type.system2client.mounts, &om->list);
}

static void remove_osns_mount(struct osns_connection_s *oc, struct osns_mount_s *om)
{
    remove_list_element(&om->list);
}

/* TODO:
    osns_system and osns_client can be in different mount namespaces
    in that case the mountpoint created in the namespace of osns_system may not be visible/exist
    in the namespace osns_client is in
    it's one of the goals to make osns_client have a different namespace (user and mount)
    */

static int test_difference_namespace(pid_t a, pid_t b)
{

    /* 20220401: for now a dummy, TO BE DONE
    under Linux for OSNS compare the symlinks of /proc/%pid%/ns/mnt and of /proc/%pid%/ns/user between the two pids
    for osns now only these two matter
    */

    return 0;
}

unsigned char test_mountpoint_is_osns_filesystem(struct fs_location_path_s *path, unsigned int major, unsigned int minor, uint64_t generation)
{
    unsigned int index=0;
    unsigned char result=0;

    while (mount_locations[index].base) {
        struct fs_location_path_s sub=FS_LOCATION_PATH_INIT;

	if (test_location_path_subdirectory(path, 'c', (void *) mount_locations[index].base, &sub)==2) {
            unsigned int len=get_unix_location_path_length(&sub);

            result=mount_locations[index].type;
            if (generation==1) break;

            if (len>0) {
                char buffer[len + 1];
                uid_t uid=(uid_t) -1;

                /* subdirectory has form like /1000/fs
                    where 1000 is the uid */

                memset(buffer, 0, len+1);
                len=copy_unix_location_path(&sub, buffer, len);
                replace_slash_char(buffer, len);
                if (skip_heading_spaces(buffer, len)) len=strlen(buffer);
                if (skip_trailing_spaces(buffer, len, SKIPSPACE_FLAG_REPLACEBYZERO)) len=strlen(buffer);

                if (sscanf(buffer, "%u fs", &uid)==1) {
                    struct connection_s *c=NULL;

                    logoutput_debug("test_mountpoint_is_osns_filesystem: found uid %u", uid);
                    c=get_osns_client_connection(&osns_server, uid);

                    /* find the client osns connection beloning to this uid */

                    if (c) {
                        struct osns_connection_s *oc=(struct osns_connection_s *)((char *) c - offsetof(struct osns_connection_s, connection));
                        struct osns_mount_s *om=find_osns_mount(oc, mount_locations[index].type);

                        logoutput_debug("test_mountpoint_is_osns_filesystem: connection with uid %u found", uid);

                        if (om) {

                            logoutput_debug("test_mountpoint_is_osns_filesystem: mount type %u found (major %u minor %u)", mount_locations[index].type, major, minor);

                            om->major=major;
                            om->minor=minor;
                            signal_set_flag(om->signal, &om->status, OSNS_MOUNT_STATUS_MOUNTINFO);

                        }

                    }

                }

            }

            break;

        }

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

static void set_interrupted_default(struct fuse_receive_s *r, uint64_t unique)
{
}

/* connect the fuse interface with the target: the VFS/kernel */

static struct osns_mount_s *mount_fuse_filesystem(struct osns_connection_s *oc, struct shared_signal_s *signal, unsigned char type, unsigned int maxread, unsigned int *p_status)
{
    struct osns_mount_s *om=NULL;
    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
    struct connection_s *connection=&oc->connection;
    uid_t uid=connection->ops.client.peer.local.uid; /* the uid for which the mount(s) will be done */
    uid_t gid=connection->ops.client.peer.local.gid;
    pid_t pid=connection->ops.client.peer.local.pid; /* the pid of the remote client */
    unsigned int size=get_mount_location(&path, type, uid, NULL, NULL);
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
    size=get_mount_location(&path, type, uid, &source, &fstype);
    signal_lock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);
    om=find_osns_mount(oc, type);

    if (om) {

	if (om->status & OSNS_MOUNT_STATUS_MOUNTED) {

	    logoutput_warning("mount_fuse_filesystem: type %u already mounted for %u:%u", type, uid, gid);
	    if (p_status) *p_status=OSNS_STATUS_ALREADYMOUNTED;
	    om=NULL;
	    goto unlockmounts;

	}

    }

    if (om==NULL) {
        struct osns_socket_s *sock=NULL;
        // void *buffer=NULL;

	om=malloc(sizeof(struct osns_mount_s));
	if (om==NULL) {

	    logoutput_warning("mount_fuse_filesystem: not able to allocate osns mount");
	    if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
	    goto exitfailed;

	}

        memset(om, 0, sizeof(struct osns_mount_s));
	om->status=OSNS_MOUNT_STATUS_INIT;

        om->buffer=malloc(SYSTEM_FUSE_RECV_BUFFER_SIZE);

        if (om->buffer) {

            om->size=SYSTEM_FUSE_RECV_BUFFER_SIZE;

        // if (posix_memalign(&buffer, getpagesize(), SYSTEM_FUSE_RECV_BUFFER_SIZE)==0) {

            //om->buffer=(char *)buffer;
            // om->size=SYSTEM_FUSE_RECV_BUFFER_SIZE;

        } else {

            logoutput_warning("mount_fuse_filesystem: unable to allocate %u butes", SYSTEM_FUSE_RECV_BUFFER_SIZE);
            goto exitfailed;

        }

	om->type=type;
	om->signal=signal;
	init_list_element(&om->list, NULL);

	om->receive.status=0;
	om->receive.flags=0;
	om->receive.ptr=NULL;
	om->receive.loop=get_default_mainloop();
	om->receive.signal=signal;

	om->receive.process_data=osns_system_process_fuse_data;
	om->receive.close=osns_system_process_fuse_close;
	om->receive.error=osns_system_process_fuse_error;
	om->receive.set_interrupted=set_interrupted_default;

	om->receive.notify_VFS=notify_VFS_system_cb;
	om->receive.reply_VFS=reply_VFS_system_cb;
	om->receive.error_VFS=error_VFS_system_cb;

        sock=&om->sock;
        om->receive.sock=sock;

    }

    /* here test the process this mount is done for is in the same namespace or not
	if not then make this thread join that namespace temporarly to do the actual mount */

    different_namespace=test_difference_namespace(getpid(), pid);

    if (different_namespace==1) {

	/* TODO: enter clients namespace via pidfd_open and setns
	    keep the current namespace somewhere to return later to 
	    use setns (under Linux) for that */

    }

    if ((om->status & OSNS_MOUNT_STATUS_OPEN)==0) {
        struct osns_socket_s *sock=&om->sock;

	if (open_fusesocket(sock)==0) {

	    logoutput_debug("mount_fuse_filesystem: open fusesocket success");
	    om->status |= OSNS_MOUNT_STATUS_OPEN;
	    sock->signal=signal;
	    init_fuse_socket_ops(sock, om->buffer, om->size);

	} else {

	    logoutput_debug("mount_fuse_filesystem: failed to open fusesocket");
	    if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
	    goto exitnamespace;

	}

    }

    add_osns_mount(oc, om);

    if (mount_fusesocket(&path, &om->sock, uid, gid, source, fstype, maxread, different_namespace)==0) {

	logoutput_debug("mount_fuse_filesystem: %s mounted at %s", source, buffer);
	om->status |= OSNS_MOUNT_STATUS_MOUNTED;

        if (add_osns_socket_eventloop(&om->sock, NULL, (void *) &om->receive, 0)==0) {

            init_system_fuse();

        } else {

            if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
            goto exitfailed;

        }

	result=0;

    } else {

	logoutput_debug("mount_fuse_filesystem: failed to mount %s at %s", source, buffer);
	if (p_status) *p_status=OSNS_STATUS_SYSTEMERROR;
	goto exitfailed;

    }

    exitnamespace:

    if (different_namespace==1) {

	/* TODO: leave the clients namespace ... in practice return to orginal namespace */

    }

    unlockmounts:
    signal_unlock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);
    return om;

    exitfailed:

    if (different_namespace==1) {

	/* TODO: leave the clients namespace ... in practice return to orginal namespace */

    }

    if (om) {

        if (om->buffer) free(om->buffer);
        (* om->sock.close)(&om->sock);
        free(om);

    }

    signal_unlock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);
    return NULL;

}

void umount_one_fuse_fs(struct osns_connection_s *oc, struct osns_mount_s *om)
{
    struct connection_s *connection=&oc->connection;
    uid_t uid=connection->ops.client.peer.local.uid; /* the uid for which the mount(s) will be done */
    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
    struct osns_socket_s *sock=&om->sock;
    unsigned int size=0;

    if ((om->status & OSNS_MOUNT_STATUS_MOUNTED)==0) return;
    size=get_mount_location(&path, om->type, uid, NULL, NULL);

    if (size>0) {
	char buffer[size+1];

	/* create the mountpath again */

	memset(buffer, 0, size+1);
	assign_buffer_location_path(&path, buffer, size+1);
	size=get_mount_location(&path, om->type, uid, NULL, NULL);

	logoutput_debug("umount_one_fuse_fs: %s", buffer);
	umount_path(&path);
	om->status &= ~OSNS_MOUNT_STATUS_MOUNTED;

    }

    (* sock->close)(sock);

}

int umount_fuse_filesystem(struct osns_connection_s *oc, struct shared_signal_s *signal, unsigned char type)
{
    struct osns_mount_s *om=NULL;
    int result=0;

    signal_lock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);

    om=find_osns_mount(oc, type);

    if (om) {

	umount_one_fuse_fs(oc, om);
	remove_list_element(&om->list);
	if (om->buffer) free(om->buffer);
	free(om);
	result=1;

    }

    signal_unlock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);
    return result;
}

void umount_all_fuse_filesystems(struct osns_connection_s *oc, struct shared_signal_s *signal)
{
    struct list_element_s *list=NULL;

    logoutput_debug("umount_all_fuse_filesystems");

    signal_unlock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);

    list=remove_list_head(&oc->type.system2client.mounts);

    while (list) {
	struct osns_mount_s *om=(struct osns_mount_s *)((char *) list - offsetof(struct osns_mount_s, list));

	umount_one_fuse_fs(oc, om);
	remove_list_element(&om->list);
	if (om->buffer) free(om->buffer);
	free(om);

	list=remove_list_head(&oc->type.system2client.mounts);

    }

    signal_unlock_flag(signal, &oc->flags, OSNS_CONNECTION_FLAG_MOUNTLIST);
}

void process_osns_mountcmd(struct osns_connection_s *oc, struct osns_in_header_s *h, struct osns_mountcmd_s *mountcmd)
{
    unsigned int status=OSNS_STATUS_NOTFOUND;
    struct osns_mount_s *om=NULL;
    struct system_timespec_s expire;

    if (find_valid_mount_location_type(mountcmd->type)==0) {

	status=OSNS_STATUS_INVALIDFLAGS;
	logoutput_debug("process_osns_mountcmd: received invalid type %u", mountcmd->type);
	goto errorout;

    }

    /* some protection here while sending the osns mount */

    om=mount_fuse_filesystem(oc, oc->signal, mountcmd->type, mountcmd->maxread, &status);
    if (om==NULL) goto errorout;

    /* wait for the init phase to complete ... INIT or ERROR ... */

    get_current_time_system_time(&expire);
    system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 8);

    if (signal_wait_flag_set(om->signal, &om->status, OSNS_MOUNT_STATUS_MOUNTINFO, &expire)==0) {

	logoutput_debug("process_msg_mountcmd: system reported mounted");

    } else {

	logoutput_debug("process_msg_mountcmd: expired waiting system reports mounted");
	status=OSNS_STATUS_SYSTEMERROR;
	goto errorout;

    }

    if (signal_wait_flag_set(om->signal, &om->receive.flags, FUSE_RECEIVE_FLAG_INIT, &expire)==0) {

	logoutput_debug("process_msg_mountcmd: init phase completed");

    } else {

	logoutput_debug("process_msg_mountcmd: init phase not completed (%s)", ((om->receive.flags & FUSE_RECEIVE_FLAG_ERROR) ? "error" : "unknown reason"));
	status=OSNS_STATUS_SYSTEMERROR;
	goto errorout;

    }

    if (osns_reply_mounted(oc, h->id, om->major, om->minor, &om->sock)>0) {

	logoutput_debug("process_msg_mountcmd: send mounted");

    } else {

	logoutput_debug("process_msg_mountcmd: failed to send mounted");

    }

    remove_osns_socket_eventloop(&om->sock);
    return;

    errorout:

    if (om) {
        struct osns_socket_s *sock=&om->sock;

        remove_osns_socket_eventloop(sock);
        (* sock->close)(sock);

    }

    osns_reply_status(oc, h->id, status, NULL, 0);
}

void process_osns_umountcmd(struct osns_connection_s *oc, struct osns_in_header_s *h, struct osns_umountcmd_s *umountcmd)
{
    unsigned int status=OSNS_STATUS_NOTFOUND;

    if (find_valid_mount_location_type(umountcmd->type)==0) {

	status=OSNS_STATUS_INVALIDFLAGS;
	logoutput_debug("process_osns_mountcmd: received invalid type %u", umountcmd->type);
	goto errorout;

    }

    if (umount_fuse_filesystem(oc, oc->signal, umountcmd->type)) {

	if (osns_reply_status(oc, h->id, 0, NULL, 0)>0) {

	    logoutput_debug("process_osns_umountcmd: send umounted");

	} else {

	    logoutput_debug("process_osns_umountcmd: failed to send umounted");

	}

	return;

    }

    errorout:
    osns_reply_status(oc, h->id, status, NULL, 0);
}

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

#include "libosns-log.h"
#include "libosns-list.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"
#include "libosns-socket.h"
#include "libosns-fuse.h"

#include "lib/fuse/utils-public.h"

#include "lib/system/path.h"

#include "osns-protocol.h"
#include "system.h"
#include "mount.h"

static struct system_stat_s rootstat;

int reply_VFS_error(struct fuse_receive_s *r, uint64_t unique, unsigned int errcode)
{
    return (* r->error_VFS)(r, unique, errcode);
}

int reply_VFS_data(struct fuse_receive_s *r, uint64_t unique, char *data, unsigned int len)
{
    return (* r->reply_VFS)(r, unique, data, len);
}

static void fuse_fs_noop(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
}

static void fuse_fs_reply_error_enoent(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    reply_VFS_error(r, inh->unique, ENOENT);
}

static void fuse_fs_getattr(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{

    if (inh->nodeid==FUSE_ROOT_ID) {
	struct fuse_attr_out out;
	struct system_timespec_s timeout;

	set_default_fuse_timeout(&timeout, 0);
	memset(&out, 0, sizeof(struct fuse_attr_out));

	fill_fuse_attr_system_stat(&out.attr, &rootstat);
	out.attr_valid=get_system_time_sec(&timeout);
	out.attr_valid_nsec=get_system_time_nsec(&timeout);

	(* r->reply_VFS)(r, inh->unique, (char *) &out, sizeof(struct fuse_attr_out));
	return;

    }

    reply_VFS_error(r, inh->unique, ENOENT);

}

static void fuse_fs_setattr(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    unsigned int errcode=ENOENT;

    if (inh->nodeid==FUSE_ROOT_ID) errcode=EPERM;
    reply_VFS_error(r, inh->unique, errcode);
}

static void fuse_fs_reply_error_eperm(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    reply_VFS_error(r, inh->unique, EPERM);
}

static void fuse_fs_reply_error_enosys(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    reply_VFS_error(r, inh->unique, ENOSYS);
}

static void fuse_fs_reply_error_eio(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    reply_VFS_error(r, inh->unique, EIO);
}

static void fuse_fs_reply_error_enodata(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    reply_VFS_error(r, inh->unique, ENODATA);
}

static void fuse_fs_opendir(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{

    if (inh->nodeid==FUSE_ROOT_ID) {
	struct fuse_open_out out;

	memset(&out, 0, sizeof(struct fuse_open_out));
	out.fh=0;
	out.open_flags=0;

	reply_VFS_data(r, inh->unique, (char *) &out, sizeof(struct fuse_open_out));
	return;

    }

    reply_VFS_error(r, inh->unique, ENOENT);
}

static void fuse_fs_readdir(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{

    if (inh->nodeid==FUSE_ROOT_ID) {
	struct fuse_read_in *inr=(struct fuse_read_in *) data;
	char tmp[inr->size];
	struct direntry_buffer_s buffer;

	buffer.data=tmp;
	buffer.pos=0;
	buffer.size=inr->size;
	buffer.offset=0;

	if (inr->offset==0) {
	    struct name_s xname=INIT_NAME;

	    xname.name=".";
	    xname.len=1;
	    add_direntry_buffer(NULL, &buffer, &xname, &rootstat);

	    xname.name="..";
	    xname.len=2;
	    add_direntry_buffer(NULL, &buffer, &xname, &rootstat);

	}

	reply_VFS_data(r, inh->unique, buffer.data, buffer.pos);
	return;

    }

    reply_VFS_error(r, inh->unique, ENOENT);

}

static void fuse_fs_reply_ok(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    reply_VFS_error(r, inh->unique, 0);
}

static void fuse_fs_statfs(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{

    if (inh->nodeid==FUSE_ROOT_ID) {
	struct fuse_statfs_out out;

	memset(&out, 0, sizeof(struct fuse_statfs_out));

	out.st.blocks=0;
	out.st.bfree=0;
	out.st.bavail=0;
	out.st.bsize=4096;

	out.st.files=0;
	out.st.ffree=0;

	out.st.namelen=255;
	out.st.frsize=4096;

	reply_VFS_data(r, inh->unique, (char *) &out, sizeof(struct fuse_statfs_out));
	return;

    }

    reply_VFS_error(r, inh->unique, ENOENT);
}

void osns_system_process_fuse_close(struct fuse_receive_s *r, unsigned int level)
{
    logoutput_debug("osns_system_process_fuse_close");
}

void osns_system_process_fuse_error(struct fuse_receive_s *r, unsigned int level, unsigned int errcode)
{
    logoutput_debug("osns_system_process_fuse_error");
}

/* process the very first stage of the fuse connection with the kernel ...
    initialize by setting the flags and parameters for the different supported filesystems (devices, network, ...)
    if this phase is done as expected hand the processing of fuse requests over to the client */

void osns_system_process_fuse_data(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{

    logoutput_debug("osns_system_process_fuse_data: code %u node id %lu unique %lu", inh->opcode, inh->nodeid, inh->unique);

    switch (inh->opcode) {

	case FUSE_INIT:
	{
	    unsigned char supported=0;
	    struct osns_mount_s *om=(struct osns_mount_s *) ((char *) r - offsetof(struct osns_mount_s, receive));

	    if (r->flags & FUSE_RECEIVE_FLAG_INIT) goto disconnect;

	    if (om->type==OSNS_MOUNT_TYPE_NETWORK) {

		supported=FUSE_FS_PROFILE_NETWORK;

	    } else if (om->type==OSNS_MOUNT_TYPE_DEVICES) {

		supported=FUSE_FS_PROFILE_DEVICES;

	    }

	    if (fuse_fs_init(r, inh, data, supported)>=0) {

                signal_set_flag(r->signal, &r->flags, FUSE_RECEIVE_FLAG_INIT);

	    } else {

		goto disconnect;

	    }

	    break;

	}

	case FUSE_DESTROY:
	case FUSE_FORGET:
	case FUSE_BATCH_FORGET:
	case FUSE_INTERRUPT:

	    fuse_fs_noop(r, inh, data);
	    break;

	case FUSE_GETATTR:

	    fuse_fs_getattr(r, inh, data);
	    break;

	case FUSE_SETATTR:
	case FUSE_MKDIR:
	case FUSE_MKNOD:
	case FUSE_SYMLINK:
	case FUSE_RMDIR:
	case FUSE_UNLINK:
	case FUSE_LINK:
	case FUSE_OPEN:
	case FUSE_CREATE:

	    fuse_fs_reply_error_eperm(r, inh, data);
	    break;

	case FUSE_READ:
	case FUSE_WRITE:
	case FUSE_FSYNC:
	case FUSE_FLUSH:
	case FUSE_LSEEK:
	case FUSE_RELEASE:
	case FUSE_GETLK:
	case FUSE_SETLK:
	case FUSE_SETLKW:

	    fuse_fs_reply_error_eio(r, inh, data);
	    break;

	case FUSE_OPENDIR:

	    fuse_fs_opendir(r, inh, data);
	    break;

	case FUSE_READDIR:

	    fuse_fs_readdir(r, inh, data);
	    break;

	case FUSE_READDIRPLUS:

	    fuse_fs_reply_error_enosys(r, inh, data);
	    break;

	case FUSE_FSYNCDIR:
	case FUSE_RELEASEDIR:

	    fuse_fs_reply_ok(r, inh, data);
	    break;

	case FUSE_LISTXATTR:
	case FUSE_GETXATTR:
	case FUSE_SETXATTR:
	case FUSE_REMOVEXATTR:

	    fuse_fs_reply_error_enodata(r, inh, data);
	    break;

	case FUSE_ACCESS:

	    fuse_fs_reply_ok(r, inh, data);
	    break;

	default:

	    fuse_fs_reply_error_enosys(r, inh, data);
	    logoutput_warning("osns_system_process_fuse_data: received not supported code %u", inh->opcode);

    }

    return;
    disconnect:
    osns_system_process_fuse_close(r, 0);

}

void init_system_fuse()
{
    set_rootstat(&rootstat);
}

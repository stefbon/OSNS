/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"

#include "init.h"
#include "attr.h"
#include "send.h"
#include "handle.h"
#include "path.h"

#define SFTP_READDIR_NAMES_SIZE		4096

static unsigned int default_mask=(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_SIZE);

static void _sftp_op_opendir(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, struct fs_path_s *location)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    int result=0;
    struct system_stat_s st;
    struct fs_handle_s *handle=NULL;
    struct fs_socket_s *sock=NULL;
    struct system_dev_s dev;

    logoutput("_sftp_op_opendir");
    memset(&st, 0, sizeof(struct system_stat_s));
    result=system_getstat(location, SYSTEM_STAT_BASIC_STATS, &st);

    if (result<0) {

	result=abs(result);

	if (result==ENOENT) {

	    status=SSH_FX_NO_SUCH_FILE;

	} else {

	    status=SSH_FX_FAILURE;

	}

	goto error;

    } else if (! system_stat_test_ISDIR(&st)) {

	status=SSH_FX_NOT_A_DIRECTORY;
	goto error;

    }

    get_dev_system_stat(&st, &dev);
    handle=sftp_create_fs_handle(sftp, get_unique_system_dev(&dev), get_ino_system_stat(&st), 0, 0, "dir");

    if (handle==NULL) {

	status=SSH_FX_FAILURE;
	goto error;

    }

    sock=&handle->socket;

    if ((* sock->ops.open)(NULL, location, sock, 0, NULL)==-1) {

	status=SSH_FX_FAILURE;
	goto error;

    }

    result=(*sock->ops.type.dir.read_dentry)(sock, NULL);
    if (result==0 || result==-1) handle->flags |= HANDLE_FLAG_EOD;
    if (send_sftp_handle(sftp, inh, handle)==-1) logoutput("_sftp_op_opendir: error sending handle reply");
    return;

    error:
    free_fs_handle(&handle);
    reply_sftp_status_simple(sftp, inh->id, status);
    logoutput("_sftp_op_opendir: status %i", status);

}

/* SSH_FXP_OPENDIR
    message has the form:
    - byte 				SSH_FXP_OPENDIR
    - uint32				id
    - string				path
*/

void sftp_op_opendir(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput_debug("sftp_op_opendir (tid %u)", (int) gettid());

    /* message should at least have 4 bytes for the path string
	note an empty path is possible */

    if (inh->len>=4) {
	char *buffer=data;
	unsigned int pos=0;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&buffer[pos]);
	pos+=4;
	path.ptr=&buffer[pos];
	pos+=path.len;

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero) */

	if (inh->len >= path.len + 4) {
	    struct fs_path_s location;
	    struct convert_sftp_path_s convert;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];

            memset(tmp, 0, size+1);
            fs_path_assign_buffer(&location, tmp, size+1);
	    (* convert.complete)(sftp, &path, &location);
	    _sftp_op_opendir(sftp, inh, &location);
	    return;

	}

    }

    error:

    logoutput_debug("sftp_op_opendir: status %u", status);
    reply_sftp_status_simple(sftp, inh->id, status);

}

static int fs_path_compare_prefix(struct fs_path_s *target, struct ssh_string_s *prefix)
{
    char *pstart=&target->buffer[target->start];
    unsigned int len=prefix->len;
    int result=-1;

    if ((target->len==len) || ((target->len > prefix->len) && (target->buffer[target->start + len]=='/')))
        result=memcmp(pstart, prefix->ptr, prefix->len);

    return result;
}

static int construct_full_path(struct fs_socket_s *sock, struct fs_path_s *path, struct fs_path_s *result)
{

#ifdef __linux__

    char procpath[64];
    int tmp=snprintf(procpath, 64, "/proc/%u/fd/%u", getpid(), sock->fd);
    struct fs_path_s symlink=FS_PATH_INIT;

    fs_path_assign_buffer(&symlink, procpath, 64);

    if (fs_path_get_target_unix_symlink(&symlink, result)==0) {

        return fs_path_append_raw(result, &path->buffer[path->start], path->len);

    }

#endif

    return 0;

}

static unsigned int get_write_max_buffersize(struct attr_context_s *actx, struct rw_attr_result_s *r, struct sftp_valid_s *valid)
{
    unsigned int size=0;
    unsigned char version=(* actx->get_sftp_protocol_version)(actx);

    /* filename as ssh string */

    size = 4 + (* actx->maxlength_filename)(actx);

    /* with version 3 a longname is written also (here empty string -> 4 bytes) */

    if (version==3) size+=4;

    /* attributes: valid (=4 bytes) plus max size attributes */

    size += 4 + get_size_buffer_write_attributes(actx, r, valid);
    return size;

}

static int check_target_symlink(struct sftp_subsystem_s *sftp, struct fs_socket_s *sock, struct fs_path_s *path)
{
    struct fs_path_s target=FS_PATH_INIT;
    int len=0;
    int result=-1;

    len=(* sock->ops.type.dir.readlinkat)(sock, path, &target);

    if (len>0) {

	if (target.buffer[0]=='/') {

            result=fs_path_compare_prefix(&target, &sftp->prefix.path);

	} else {
	    struct fs_path_s tmp=FS_PATH_INIT;

            if (construct_full_path(sock, &target, &tmp)>0) {

                result=fs_path_compare_prefix(&tmp, &sftp->prefix.path);
                fs_path_clear(&tmp);

            }

	}

    }

    fs_path_clear(&target);
    return result;

}

static void _sftp_op_readdir(struct sftp_subsystem_s *sftp, struct fs_handle_s *handle, struct sftp_in_header_s *inh)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;
    unsigned int error=0;
    char buffer[SFTP_READDIR_NAMES_SIZE]; 					/* make this configurable? -> desired buffer between client and server */
    unsigned int pos=0;
    unsigned int count=0;
    unsigned char eof=0;
    struct attr_context_s *actx=&sftp->attrctx;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct sftp_valid_s *valid=sftp_get_valid_fs_handle(handle);
    unsigned int len=get_write_max_buffersize(actx, &r, valid);
    unsigned int mask=r.stat_mask;
    struct fs_socket_s *sock=&handle->socket;

    logoutput("_sftp_op_readdir (tid %u) valid %u:%u len write buffer %u stat mask %u", gettid(), valid->mask, valid->flags, len, mask);

    while ((pos < SFTP_READDIR_NAMES_SIZE) && ((handle->flags & HANDLE_FLAG_EOD)==0)) {
        struct fs_dentry_s dentry;

        if ((* sock->ops.type.dir.get_dentry)(sock, &dentry)==1) {
            struct system_stat_s stat;
            struct fs_path_s path=FS_PATH_INIT;
            int result=0;

	    memset(&stat, 0, sizeof(struct system_stat_s));
	    fs_path_assign_buffer(&path, dentry.name, dentry.len);

	    if ((* sock->ops.type.dir.fstatat)(sock, &path, mask, &stat, 0)==0) {
	        struct ssh_string_s name=SSH_STRING_INIT;
                char tmp[len]; 								/* temporary buffer for writing filename and attributes */
                struct attr_buffer_s abuff;
                unsigned int size=0;

                set_attr_buffer_write(&abuff, tmp, len);

                /* TODO:
                    - add check dentry is readable
                    (add an option for that like readdir.ignore.nonreadable-unix.) */

                /* test the symlink target does exist ... do not list broken symlinks */

		if (system_stat_test_ISLNK(&stat) && (sftp->prefix.flags & (SFTP_PREFIX_FLAG_IGNORE_XDEV_SYMLINKS))) {

		    /* if target points outside */

		    if (check_target_symlink(sftp, sock, &path)==0) continue;

		}

		/* write name as ssh string to temporary buffer */

		name.len=dentry.len;
		name.ptr=dentry.name;

		(* actx->ops.write_name_name_response)(actx, &abuff, &name);
		(* abuff.ops->rw.write.write_uint32)(&abuff, (valid->mask | valid->flags));
		write_attributes_generic(actx, &abuff, &r, &stat, valid);
		size=(unsigned int)(abuff.pos);
		logoutput("sftp_op_readdir: found %s size %i pos %i max %i", dentry.name, size, pos, SFTP_READDIR_NAMES_SIZE);

		/* does it fit? */

		if ((pos + size) > SFTP_READDIR_NAMES_SIZE) break;

		memcpy(&buffer[pos], abuff.buffer, size);
		pos+=size;
		count++;

		/* reset abuff */

		reset_attr_buffer_write(&abuff);
		memset(tmp, 0, len);

	    } /* fstatat did not find dentry... ignore */

            result=(* sock->ops.type.dir.read_dentry)(sock, &dentry);

            if (result==-1 || result==0) {

                handle->flags |= HANDLE_FLAG_EOD;
                break;

            }

	}

    }

    if (handle->flags & HANDLE_FLAG_EOD) {

        if (pos==0) {

	    status=SSH_FX_EOF;
	    goto errorout;

	}

	eof=1;

    }

    finish:

    logoutput_debug("sftp_op_readdir: reply count %i pos %i", count, pos);
    if (reply_sftp_names(sftp, inh->id, count, buffer, pos, eof)==-1) logoutput_warning("sftp_op_readdir: error sending readdir names");
    return;

    errorout:

    reply_sftp_status_simple(sftp, inh->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}

/* SSH_FXP_READDIR
    message has the form:
    - byte 				SSH_FXP_READDIR
    - uint32				id
    - string				handle
*/

void sftp_op_readdir(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_readdir (tid %u)", gettid());

    /* handle is 16 bytes long, so message is 4 + 16 = 20 bytes */

    if (inh->len >= 4 + get_fs_handle_buffer_size()) {
	unsigned int len=0;

	len=get_uint32(&data[0]);

	if (len>=get_fs_handle_buffer_size()) {
	    unsigned int error=0;
	    struct fs_handle_s *handle=NULL;

            handle=get_fs_handle(sftp->connection.unique, &data[4], len, NULL);

	    if ((handle==NULL) || (handle->type != FS_HANDLE_TYPE_DIR)) {

		logoutput_debug("sftp_op_readdir: handle not found");
		status=SSH_FX_INVALID_HANDLE;

	    }

	    if (handle->flags & HANDLE_FLAG_EOD) {

		status=SSH_FX_EOF;
		goto out;

	    }

	    _sftp_op_readdir(sftp, handle, inh);
	    return;

	}

    }

    out:
    reply_sftp_status_simple(sftp, inh->id, status);
    return;

    disconnect:
    finish_sftp_subsystem(sftp);
}

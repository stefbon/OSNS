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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <pwd.h>
#include <sys/syscall.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "init.h"
#include "attr.h"
#include "send.h"
#include "handle.h"
#include "path.h"

#define SFTP_READDIR_NAMES_SIZE		4096

static unsigned int default_mask=(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_SIZE);

static int send_sftp_dirhandle(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, struct commonhandle_s *handle)
{
    unsigned int size=(unsigned int) get_sftp_handle_size();
    char bytes[size];

    logoutput_info("send_sftp_dirhandle");
    write_sftp_commonhandle(handle, bytes, size);
    return reply_sftp_handle(sftp, payload->id, bytes, size);
}

static void _sftp_op_opendir(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, struct fs_location_s *location)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    int result=0;
    struct system_stat_s st;
    struct commonhandle_s *handle=NULL;
    struct fs_location_devino_s devino=FS_LOCATION_DEVINO_INIT;
    struct system_dev_s dev;

    logoutput("_sftp_op_opendir: path %.*s", location->type.path.len, location->type.path.ptr);

    memset(&st, 0, sizeof(struct system_stat_s));

    result=system_getstat(&location->type.path, SYSTEM_STAT_BASIC_STATS, &st);

    if (result<0) {

	result=abs(result);

	if (result==ENOENT) {

	    status=SSH_FX_NO_SUCH_FILE;

	} else {

	    status=SSH_FX_FAILURE;

	}

	goto error;

    } else if (! S_ISDIR(get_type_system_stat(&st))) {

	status=SSH_FX_NOT_A_DIRECTORY;
	goto error;

    }

    logoutput_debug("_sftp_op_opendir: insert dirhandle");
    devino.ino=get_ino_system_stat(&st);
    get_dev_system_stat(&st, &dev);
    devino.dev=get_unique_system_dev(&dev);
    handle=create_sftp_dirhandle(sftp, &devino);

    if (handle==NULL) {

	status=SSH_FX_FAILURE;
	goto error;

    } else {
	struct dirhandle_s *dh=&handle->type.dir;

	if ((* dh->open)(dh, location, 0)==-1) {

	    status=SSH_FX_FAILURE;
	    goto error;

	}

	if (send_sftp_dirhandle(sftp, payload, handle)==-1) logoutput("_sftp_op_opendir: error sending handle reply");
	return;

    }

    error:

    logoutput("_sftp_op_opendir: status %i", status);
    free_commonhandle(&handle);
    reply_sftp_status_simple(sftp, payload->id, status);

}

/* SSH_FXP_OPENDIR
    message has the form:
    - byte 				SSH_FXP_OPENDIR
    - uint32				id
    - string				path
    */

void sftp_op_opendir(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_opendir (%i)", (int) gettid());

    /* message should at least have 4 bytes for the path string
	note an empty path is possible */

    if (payload->len>=4) {
	char *buffer=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	len=get_uint32(&buffer[pos]);
	pos+=4;

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero) */

	if (payload->len >= len + 4) {
	    struct sftp_identity_s *user=&sftp->identity;
	    struct ssh_string_s path={.len=len, .ptr=&buffer[pos]};
	    struct fs_location_s location;
	    struct convert_sftp_path_s convert={NULL};
	    unsigned int size=get_fullpath_size(user, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];
	    unsigned int error=0;

	    memset(&location, 0, sizeof(struct fs_location_s));
	    location.flags=FS_LOCATION_FLAG_PATH;
	    set_buffer_location_path(&location.type.path, tmp, size+1, 0);
	    (* convert.complete)(user, &path, &location.type.path);
	    pos+=len;

	    _sftp_op_opendir(sftp, payload, &location);
	    return;

	}

    }

    error:

    logoutput("sftp_op_opendir: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

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

    logoutput_debug("get_write_max_buffersize: size %i stat mask %i valid %i ignored %i", size, r->stat_mask, r->valid.mask, r->ignored);

    return size;

}

static void _sftp_op_readdir(struct sftp_subsystem_s *sftp, struct commonhandle_s *handle, struct sftp_payload_s *payload)
{
    struct dirhandle_s *dh=&handle->type.dir;
    unsigned int status=SSH_FX_BAD_MESSAGE;
    unsigned int error=0;
    char buffer[SFTP_READDIR_NAMES_SIZE]; 					/* make this configurable? -> desired buffer between client and server */
    unsigned int pos=0;
    unsigned int count=0;
    unsigned char eof=0;
    struct attr_context_s *actx=&sftp->attrctx;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct sftp_valid_s *valid=get_valid_sftp_dirhandle(handle);
    unsigned int len=get_write_max_buffersize(actx, &r, valid);
    unsigned int size=0;
    unsigned int mask=r.stat_mask;
    char tmp[len]; 								/* temporary buffer for writing filename and attributes */
    struct attr_buffer_s abuff;
    struct fs_dentry_s *dentry=NULL;
    struct system_stat_s stat;
    struct ssh_string_s name=SSH_STRING_INIT;

    set_attr_buffer_write(&abuff, tmp, len);

    logoutput("_sftp_op_readdir (tid %i) valid %i len write buffer %i stat mask %i", (int) gettid(), valid, len, mask);

    while (pos < SFTP_READDIR_NAMES_SIZE) {

	dentry=(* dh->readdentry)(dh);

	if (dentry) {

	    if ((* dh->fstatat)(dh, dentry->name, mask, &stat)==0) {

		/* write name as ssh string to temporary buffer */

		name.len=dentry->len;
		name.ptr=dentry->name;

		(* actx->ops.write_name_name_response)(actx, &abuff, &name);
		size=(unsigned int)(abuff.pos - abuff.buffer);
		logoutput("sftp_op_readdir: a. found %s %i bytes written", dentry->name, size);

		(* abuff.ops->rw.write.write_uint32)(&abuff, valid->mask);
		size=(unsigned int)(abuff.pos - abuff.buffer);
		logoutput("sftp_op_readdir: b. %i bytes written", size);

		write_attributes_generic(actx, &abuff, &r, &stat, valid);
		size=(unsigned int)(abuff.pos - abuff.buffer);
		logoutput("sftp_op_readdir: c. %i bytes written pos %i", size, pos);

		/* does it fit? */

		if ((pos + size) > SFTP_READDIR_NAMES_SIZE) {

		    (* dh->set_keep_dentry)(dh);
		    /* 20211020: also keep the buffer with attributes just written? (where?) or */
		    break;

		}

		memcpy(&buffer[pos], abuff.buffer, abuff.len);
		pos+=size;
		count++;

		/* reset abuff */

		reset_attr_buffer_write(&abuff);

	    } /* fstatat did not find dentry... ignore */

	} else {

	    /* no dentry caused by? */

	    if (dh->flags & DIRHANDLE_FLAG_ERROR) {

		status=SSH_FX_FAILURE; /* todo: match the error in dh to a sftp error better */
		goto errorout;

	    } else if (dh->flags & DIRHANDLE_FLAG_EOD) {

		if (pos==0) {

		    status=SSH_FX_EOF;
		    goto errorout;

		}

		eof=1;
		break;

	    }

	}

    }

    finish:

    logoutput_debug("sftp_op_readdir: reply count %i pos %i", count, pos);
    if (reply_sftp_names(sftp, payload->id, count, buffer, pos, eof)==-1) logoutput_warning("sftp_op_readdir: error sending readdir names");
    return;

    errorout:

    reply_sftp_status_simple(sftp, payload->id, status);
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

void sftp_op_readdir(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_readdir (%i)", (int) gettid());

    /* handle is 16 bytes long, so message is 4 + 16 = 20 bytes */

    if (payload->len >= 4 + get_sftp_handle_size()) {
	unsigned int len=0;
	char *data=payload->data;

	len=get_uint32(&data[0]);
	logoutput("sftp_op_readdir (%i) len %i", (int) gettid(), len);

	if (len>=get_sftp_handle_size()) {
	    unsigned int error=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[4], len, NULL);

	    if (handle) {
		struct dirhandle_s *dh=&handle->type.dir;

		if (dh->flags & DIRHANDLE_FLAG_ERROR) {

		    status=SSH_FX_FAILURE;
		    goto out;

		} else if (dh->flags & DIRHANDLE_FLAG_EOD) {

		    status=SSH_FX_EOF;
		    goto out;

		}

		_sftp_op_readdir(sftp, handle, payload);
		return;

	    } else {

		if (error==EPERM) {

		    /* serious error: client wants to use a handle he has no permissions for */

		    logoutput("sftp_op_readdir: client has no permissions to use handle");
		    goto disconnect;

		}

		logoutput("sftp_op_readdir: handle not found");
		status=SSH_FX_INVALID_HANDLE;

	    }

	}

    }

    out:
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:
    finish_sftp_subsystem(sftp);
}

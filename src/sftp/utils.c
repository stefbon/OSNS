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

#include <pwd.h>
#include <grp.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "file.h"
#include "threads.h"

#include "workspace-interface.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "request-hash.h"

static int sftp_open_remote_file(struct file_readfile_s *readfile, char *path)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) readfile->ptr;
    struct sftp_request_s sftp_r;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));

    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;
    sftp_r.call.open.path=(unsigned char *) path;
    sftp_r.call.open.len=strlen(path);
    sftp_r.call.open.posix_flags=O_RDONLY;

    if ((* sftp->send_ops->open)(sftp, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout(sftp, &timeout);

	if (wait_sftp_response(sftp, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct ssh_string_s *handle=&readfile->id.handle;

		/* handle name is defined in response.handle.name: take it "over" */

		handle->ptr=(char *)reply->response.handle.name;
		handle->len=reply->response.handle.len;
		reply->response.handle.name=NULL;
		reply->response.handle.len=0;
		return 0; /* success */

	    } else if (reply->type==SSH_FXP_STATUS) {

		readfile->error=reply->response.status.linux_error;
		logoutput("sftp_open_remote_file: status reply %i", readfile->error);

	    } else {

		readfile->error=EPROTO;

	    }

	}

    }

    readfile->flags |= FILE_READFILE_FLAG_ERROR;
    if (readfile->error==0) readfile->error=(sftp_r.reply.error ? sftp_r.reply.error : EIO);
    logoutput("sftp_open_remote_file: error %i:%s", readfile->error, strerror(readfile->error));
    return -1;
}

static int sftp_pread_remote_file(struct file_readfile_s *readfile, off_t off, size_t size, struct ssh_string_s *data)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) readfile->ptr;
    struct sftp_request_s sftp_r;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));

    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    sftp_r.call.read.handle=(unsigned char *) readfile->id.handle.ptr;
    sftp_r.call.read.len=readfile->id.handle.len;
    sftp_r.call.read.offset=(uint64_t) off;
    sftp_r.call.read.size=(uint64_t) readfile->size;

    if ((* sftp->send_ops->read)(sftp, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout(sftp, &timeout);

	if (wait_sftp_response(sftp, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_DATA) {

		logoutput("sftp_pread_remote_file: received %i bytes", reply->response.data.size);

		/* copy the data: readfile->buffer already allocated */

		readfile->flags &= ~FILE_READFILE_FLAG_EOF;
		readfile->error=0;
		memcpy(readfile->buffer, (char *) reply->response.data.data, reply->response.data.size);
		readfile->bytesread=reply->response.data.size;
		data->len=((readfile->bytesread<size) ? readfile->bytesread : size);
		data->ptr=readfile->buffer;

		/* possibly this is the latest chunk of data (not all protocol versions do support this) */

		if (reply->response.data.flags & SFTP_RESPONSE_FLAG_EOF_SUPPORTED) {

		    readfile->flags |= ((reply->response.data.flags & SFTP_RESPONSE_FLAG_EOF) ? FILE_READFILE_FLAG_EOF : 0);

		}

		return (int) data->len;

	    } else if (reply->type==SSH_FXP_STATUS) {

		readfile->bytesread=0;

		if (reply->response.status.linux_error==ENODATA) {

		    readfile->flags |= FILE_READFILE_FLAG_EOF;
		    return 0;

		}

		readfile->error=reply->response.status.linux_error;
		logoutput("sftp_pread_remote_file: status reply %i", readfile->error);

	    } else {

		readfile->error=EPROTO;

	    }

	}

    }

    readfile->flags |= FILE_READFILE_FLAG_ERROR;
    if (readfile->error==0) readfile->error=(sftp_r.reply.error ? sftp_r.reply.error : EIO);
    logoutput("sftp_pread_remote_file: error %i:%s", readfile->error, strerror(readfile->error));
    return -1;

}

static void sftp_close_remote_file(struct file_readfile_s *readfile)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) readfile->ptr;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    if (readfile->id.handle.ptr==NULL) return;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));

    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;
    sftp_r.call.close.handle=(unsigned char *) readfile->id.handle.ptr;
    sftp_r.call.close.len=readfile->id.handle.len;

    if ((* sftp->send_ops->close)(sftp, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout(sftp, &timeout);

	if (wait_sftp_response(sftp, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code!=0) readfile->error=reply->response.status.linux_error;

	    } else {

		readfile->error=EPROTO;

	    }

	}

    }

    if (sftp_r.reply.error) readfile->error=sftp_r.reply.error;
    if (readfile->error) logoutput("sftp_close_remote_file: error %i:%s", readfile->error, strerror(readfile->error));

    if (readfile->buffer) {

	free(readfile->buffer);
	readfile->buffer=NULL;
	readfile->size=0;
	readfile->bytesread=0;

    }

    free(readfile->id.handle.ptr);
    readfile->id.handle.ptr=NULL;
    readfile->id.handle.len=0;

}

void init_sftp_file_readfile(struct sftp_client_s *sftp, struct file_readfile_s *file)
{

    init_file_readfile(file);

    file->ptr=(void *) sftp;

    file->open=sftp_open_remote_file;
    file->pread=sftp_pread_remote_file;
    file->close=sftp_close_remote_file;

}

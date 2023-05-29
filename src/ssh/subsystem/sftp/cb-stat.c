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

#include "send.h"
#include "path.h"
#include "handle.h"
#include "init.h"
#include "attr.h"

int reply_sftp_attr_from_stat(struct sftp_subsystem_s *sftp, uint32_t id, struct sftp_valid_s *valid, struct system_stat_s *stat)
{
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    unsigned int size=get_size_buffer_write_attributes(&sftp->attrctx, &r, valid);
    char buffer[size];
    struct attr_buffer_s abuff;

    set_attr_buffer_write(&abuff, buffer, size);
    (* abuff.ops->rw.write.write_uint32)(&abuff, (r.valid.mask | r.valid.flags));
    write_attributes_generic(&sftp->attrctx, &abuff, &r, stat, valid);

    return reply_sftp_attrs(sftp, id, abuff.buffer, abuff.len);

}

/* SSH_FXP_(L)STAT/
    message has the form:
    - byte 				SSH_FXP_STAT or SSH_FXP_LSTAT
    - uint32				id
    - string				path
    - uint32				flags
    */

static void sftp_op_stat_generic(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data, int (* cb_stat)(struct fs_path_s *p, unsigned int mask, struct system_stat_s *s))
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    /* message should at least have 4 bytes for the path string, and 4 for the flags
	note an empty path is possible */

    if (inh->len>=8) {
	unsigned int pos=0;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	if (path.len + 8 <= inh->len) {
	    struct system_stat_s stat;
	    struct sftp_valid_s valid;
	    struct fs_path_s location=FS_PATH_INIT;
	    struct convert_sftp_path_s convert;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert);
	    char pathtmp[size+1];
	    int result=0;
	    unsigned int mask=0;
	    uint32_t validbits=0;

	    validbits=get_uint32(&data[pos]);
	    convert_sftp_valid_w(&sftp->attrctx, &valid, validbits);

	    fs_path_assign_buffer(&location, pathtmp, size+1);
	    (* convert.complete)(sftp, &path, &location);
	    mask=translate_valid_2_stat_mask(&sftp->attrctx, &valid, 'w');
	    result=(* cb_stat)(&location, mask, &stat);

	    logoutput("sftp_op_stat: %.*s result %i valid %i", location.len, location.buffer, result, valid.mask);

	    if (result==0) {

		if (reply_sftp_attr_from_stat(sftp, inh->id, &valid, &stat)==-1) logoutput_warning("sftp_op_stat_generic: error sending attr");
		return;

	    } else {

		result=abs(result);
		status=SSH_FX_FAILURE;

		if (result==ENOENT) {

		    status=SSH_FX_NO_SUCH_FILE;

		} else if (result==ENOTDIR) {

		    status=SSH_FX_NO_SUCH_PATH;

		} else if (result==EACCES) {

		    status=SSH_FX_PERMISSION_DENIED;

		}

	    }

	}

    }

    logoutput("sftp_op_stat_generic: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);

}

void sftp_op_stat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    sftp_op_stat_generic(sftp, inh, data, system_getstat);
}

void sftp_op_lstat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    sftp_op_stat_generic(sftp, inh, data, system_getlstat);
}

void sftp_op_fstat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_fstat (%i)", (int) gettid());

    if (inh->len >= 8 + get_fs_handle_buffer_size()) {
	unsigned int len=0;
	unsigned int pos=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_fs_handle_buffer_size()) {
	    struct system_stat_s stat;
	    struct sftp_valid_s valid;
	    int result=0;
	    unsigned int count=0;
	    struct fs_handle_s *handle=NULL;
	    struct fs_socket_s *sock=NULL;
	    unsigned int mask=0;
	    uint32_t validbits=0;

            handle=get_fs_handle(sftp->connection.unique, &data[4], len, &count);

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_fstat: handle not found");
		goto error;

	    }

	    pos+=count;
	    validbits=get_uint32(&data[pos]);
	    convert_sftp_valid_w(&sftp->attrctx, &valid, validbits);
	    mask=translate_valid_2_stat_mask(&sftp->attrctx, &valid, 'w');
	    sock=&handle->socket;

            result=(* sock->ops.fgetstat)(sock, mask, &stat);
	    logoutput("sftp_op_fstat: result %i valid %i", result, valid.mask);

	    if (result==0) {
		struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
		unsigned int size=get_size_buffer_write_attributes(&sftp->attrctx, &r, &valid);
		char tmp[size];
		struct attr_buffer_s abuff;

		set_attr_buffer_write(&abuff, tmp, size);
		(* abuff.ops->rw.write.write_uint32)(&abuff, (r.valid.mask | r.valid.flags));
		write_attributes_generic(&sftp->attrctx, &abuff, &r, &stat, &valid);
		if (reply_sftp_attrs(sftp, inh->id, tmp, abuff.len)==-1) logoutput_warning("sftp_op_stat: error sending attr");
		return;

	    } else {

		result=abs(result);
		status=SSH_FX_FAILURE;

		if (result==ENOENT) {

		    status=SSH_FX_NO_SUCH_FILE;

		} else if (result==ENOTDIR) {

		    status=SSH_FX_NO_SUCH_PATH;

		} else if (result==EACCES) {

		    status=SSH_FX_PERMISSION_DENIED;

		}

	    }

	} else {

	    logoutput_warning("sftp_op_fstat: invalid handle size %i", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_fstat: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}

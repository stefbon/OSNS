/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include <linux/fs.h>

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v05.h"
#include "sftp/attr.h"
#include "send-v03.h"
#include "send-v04.h"
#include "datatypes/ssh-uint.h"

static void get_sftp_openmode(unsigned int posix_flags, struct sftp_openmode_s *openmode)
{

    if (posix_flags & O_RDWR) {

	openmode->access |= (ACE4_READ_DATA | ACE4_READ_ATTRIBUTES | ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES);

    } else if (posix_flags & O_WRONLY) {

	openmode->access |= (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES);

    } else {

	openmode->access |= (ACE4_READ_DATA | ACE4_READ_ATTRIBUTES);

    }

    if (posix_flags & O_APPEND) {

	openmode->access |= (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_APPEND_DATA);
	openmode->flags |= SSH_FXF_APPEND_DATA_ATOMIC;

    }

    if ((posix_flags & O_CREAT) && (posix_flags & O_TRUNC)) {

	openmode->flags |= (SSH_FXF_CREATE_TRUNCATE);

    } else if (posix_flags & O_CREAT) {

	openmode->flags |= ((posix_flags & O_EXCL) ? (SSH_FXF_CREATE_NEW) : (SSH_FXF_OPEN_OR_CREATE));

    } else if (posix_flags & O_TRUNC) {

	openmode->flags |= (SSH_FXF_TRUNCATE_EXISTING);

    } else {

	openmode->flags |= (SSH_FXF_OPEN_EXISTING);

    }

}

/*
    OPEN a file
    - byte 1 	SSH_FXP_OPEN
    - uint32 	request id
    - uint32 	len path (n)
    - byte[n]	path
    - uint32	desired-access
    - uint32	flags
    - attrs (size) ignored when opening an existing file

    -------- +

    1 + 4 + 4 + len + 4 + 4 + size = 17 + len + size

*/

int send_sftp_open_v05(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    char data[21 + sftp_r->call.open.len + sftp_r->call.open.size + 5];
    unsigned int pos=0;
    struct sftp_openmode_s openmode;

    openmode.flags=0;
    openmode.access=0;

    get_sftp_openmode(sftp_r->call.open.posix_flags, &openmode);
    sftp_r->id=get_sftp_request_id(sftp);

    logoutput_debug("send_sftp_open: posix %i access %i flags %i", sftp_r->call.open.posix_flags, openmode.access, openmode.flags);

    store_uint32(&data[pos], 0);
    pos+=4;

    data[pos]=(unsigned char) SSH_FXP_OPEN;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.open.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.open.path, sftp_r->call.open.len);
    pos+=sftp_r->call.open.len;
    store_uint32(&data[pos], openmode.access);
    pos+=4;
    store_uint32(&data[pos], openmode.flags);
    pos+=4;

    if (sftp_r->call.open.size>0) {

	memcpy((char *) &data[pos], sftp_r->call.open.buff, sftp_r->call.open.size);
	pos+=sftp_r->call.open.size;

    } else {

	store_uint32(&data[pos], 0); /* valid attributes: no attributes */
	pos+=4;
	data[pos]=(unsigned char) SSH_FILEXFER_TYPE_REGULAR;
	pos++;

    }

    store_uint32(&data[0], pos-4);

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

/*
    RENAME
    - byte 1 	SSH_FXP_RENAME
    - uint32 	request id
    - uint32 	len old path (n)
    - byte[n]	old path
    - uint32 	len new path (m)
    - byte[m]	new path
    - uint32	flags
*/

int send_sftp_rename_v05(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    char data[21 + sftp_r->call.rename.len + sftp_r->call.rename.target_len];
    unsigned int pos=0;
    unsigned int flags=SSH_FXF_RENAME_NATIVE; /* seems reasonable */

    sftp_r->id=get_sftp_request_id(sftp);

    if (! (sftp_r->call.rename.posix_flags & RENAME_NOREPLACE)) {

	flags|= SSH_FXF_RENAME_OVERWRITE;

    }

    store_uint32(&data[pos], 17 + sftp_r->call.rename.len + sftp_r->call.rename.target_len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_RENAME;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.rename.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.rename.path, sftp_r->call.rename.len);
    pos+=sftp_r->call.rename.len;
    store_uint32(&data[pos], sftp_r->call.rename.target_len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.rename.target_path, sftp_r->call.rename.target_len);
    pos+=sftp_r->call.rename.target_len;
    store_uint32(&data[pos], flags);
    pos+=4;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

static int send_sftp_stat_v05(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_stat_v04_generic(sftp, sftp_r, valid);
}

static int send_sftp_lstat_v05(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_lstat_v04_generic(sftp, sftp_r, valid);
}

static int send_sftp_fstat_v05(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_fstat_v04_generic(sftp, sftp_r, valid);
}

static struct sftp_send_ops_s send_ops_v05 = {
    .version				= 5,
    .init				= send_sftp_init_v03,
    .open				= send_sftp_open_v05,
    .read				= send_sftp_read_v03,
    .write				= send_sftp_write_v03,
    .close				= send_sftp_close_v03,
    .stat				= send_sftp_stat_v05,
    .lstat				= send_sftp_lstat_v05,
    .fstat				= send_sftp_fstat_v05,
    .setstat				= send_sftp_setstat_v03,
    .fsetstat				= send_sftp_fsetstat_v03,
    .realpath				= send_sftp_realpath_v03,
    .readlink				= send_sftp_readlink_v03,
    .opendir				= send_sftp_opendir_v03,
    .readdir				= send_sftp_readdir_v03,
    .remove				= send_sftp_remove_v03,
    .rmdir				= send_sftp_rmdir_v03,
    .mkdir				= send_sftp_mkdir_v03,
    .rename				= send_sftp_rename_v05,
    .symlink				= send_sftp_symlink_v03,
    .block				= send_sftp_block_v03,
    .unblock				= send_sftp_unblock_v03,
    .extension				= send_sftp_extension_v03,
    .custom				= send_sftp_custom_v03,
};

struct sftp_send_ops_s *get_sftp_send_ops_v05()
{
    return &send_ops_v05;
}

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

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v06.h"
#include "sftp/attr.h"
#include "send-v03.h"
#include "send-v04.h"
#include "send-v05.h"
#include "datatypes/ssh-uint.h"

static unsigned int get_sftp_lockflags_posix(unsigned int type)
{
    unsigned int flags=SSH_FXF_BLOCK_ADVISORY;

    if (type==F_RDLCK) {

	flags|=SSH_FXF_BLOCK_READ;

    } else if (type==F_WRLCK) {

	flags|=SSH_FXF_BLOCK_WRITE;

    }

    return flags;

}

static unsigned int get_sftp_lockflags_flock(unsigned int type)
{
    unsigned int flags=SSH_FXF_BLOCK_ADVISORY;

    if (type & LOCK_SH) {

	/* block writes and deletes */

	flags|=(SSH_FXF_BLOCK_WRITE | SSH_FXF_BLOCK_DELETE);

    } else if (type & LOCK_EX) {

	/* block reads */

	flags|=SSH_FXF_BLOCK_READ;

    }

    return flags;

}

static int send_sftp_stat_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_stat_v04_generic(sftp, sftp_r, valid);
}

static int send_sftp_lstat_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_lstat_v04_generic(sftp, sftp_r, valid);
}

static int send_sftp_fstat_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_fstat_v04_generic(sftp, sftp_r, valid);
}

/*
    SYMLINK
    - byte 1 	SSH_FXP_LINK
    - uint32 	request id
    - uint32 	len path (n)
    - byte[n]	path
    - uint32 	len target path (m)
    - byte[m]	target path
    - byte 	is symbolic link (1)
*/

int send_sftp_symlink_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    char data[18 + sftp_r->call.link.len + sftp_r->call.link.target_len];
    unsigned int pos=0;

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 14 + sftp_r->call.link.len + sftp_r->call.link.target_len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_LINK;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.link.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.link.path, sftp_r->call.link.len);
    pos+=sftp_r->call.link.len;
    store_uint32(&data[pos], sftp_r->call.link.target_len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.link.target_path, sftp_r->call.link.target_len);
    pos+=sftp_r->call.link.target_len;
    data[pos]=1;
    pos++;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

/*
    BLOCK (byte range lock)
    - byte 1 	SSH_FXP_BLOCK
    - uint32 	request id
    - uint32 	len handle (n)
    - byte[n]	handle
    - uint64	offset
    - uint64	size
    - uint32	flags
*/

int send_sftp_block_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    char data[33 + sftp_r->call.block.len];
    unsigned int pos=0;
    unsigned int mask=get_sftp_lockflags_flock(sftp_r->call.block.type);

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 29 + sftp_r->call.block.len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_BLOCK;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.block.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.block.handle, sftp_r->call.block.len);
    pos+=sftp_r->call.block.len;
    store_uint64(&data[pos], sftp_r->call.block.offset);
    pos+=8;
    store_uint64(&data[pos], sftp_r->call.block.size);
    pos+=8;
    store_uint32(&data[pos], mask);
    pos+=4;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

/*
    UNBLOCK (byte range lock)
    - byte 1 	SSH_FXP_UNBLOCK
    - uint32 	request id
    - uint32 	len handle (n)
    - byte[n]	handle
    - uint64	offset
    - uint64	size
*/

int send_sftp_unblock_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    char data[29 + sftp_r->call.unblock.len];
    unsigned int pos=0;

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 25 + sftp_r->call.unblock.len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_UNBLOCK;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.unblock.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.unblock.handle, sftp_r->call.unblock.len);
    pos+=sftp_r->call.unblock.len;
    store_uint64(&data[pos], sftp_r->call.unblock.offset);
    pos+=8;
    store_uint64(&data[pos], sftp_r->call.unblock.size);
    pos+=8;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

/*
    REALPATH

    - byte 1		SSH_FXP_REALPATH
    - uint32		request id
    - string		original path
    - byte		control byte [optional]
    (- string		compose path [optional])
*/

/*
    realpath
    check the realpath (of a symlink for example) and ask target does exists
*/

int send_sftp_realpath_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    char data[14 + sftp_r->call.realpath.len];
    unsigned int pos=0;

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 10 + sftp_r->call.realpath.len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_REALPATH;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.realpath.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.realpath.path, sftp_r->call.realpath.len);
    pos+=sftp_r->call.realpath.len;
    data[pos]=SSH_FXP_REALPATH_STAT_ALWAYS;
    pos++;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

static struct sftp_send_ops_s send_ops_v06 = {
    .version				= 6,
    .init				= send_sftp_init_v03,
    .open				= send_sftp_open_v05,
    .read				= send_sftp_read_v03,
    .write				= send_sftp_write_v03,
    .close				= send_sftp_close_v03,
    .stat				= send_sftp_stat_v06,
    .lstat				= send_sftp_lstat_v06,
    .fstat				= send_sftp_fstat_v06,
    .setstat				= send_sftp_setstat_v03,
    .fsetstat				= send_sftp_fsetstat_v03,
    .realpath				= send_sftp_realpath_v06,
    .readlink				= send_sftp_readlink_v03,
    .opendir				= send_sftp_opendir_v03,
    .readdir				= send_sftp_readdir_v03,
    .remove				= send_sftp_remove_v03,
    .rmdir				= send_sftp_rmdir_v03,
    .mkdir				= send_sftp_mkdir_v03,
    .rename				= send_sftp_rename_v05,
    .symlink				= send_sftp_symlink_v06,
    .block				= send_sftp_block_v06,
    .unblock				= send_sftp_unblock_v06,
    .extension				= send_sftp_extension_v03,
    .custom				= send_sftp_custom_v03,
};

struct sftp_send_ops_s *get_sftp_send_ops_v06()
{
    return &send_ops_v06;
}

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
#include "sftp/protocol-v04.h"
#include "sftp/attr.h"
#include "send-v03.h"
#include "send-v04.h"
#include "datatypes/ssh-uint.h"


/*
    STAT
    - byte 1 	SSH_FXP_STAT
    - uint32 	request id
    - uint32 	len path (n)
    - byte[n]	path
    - uint32	flags which data of interest
*/

int send_sftp_stat_v04_generic(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int flags)
{
    char data[17 + sftp_r->call.stat.len];
    unsigned int pos=0;

    logoutput_debug("send_sftp_stat_v04_generic: flags %i", flags);

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 13 + sftp_r->call.stat.len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_STAT;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.stat.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.stat.path, sftp_r->call.stat.len);
    pos+=sftp_r->call.stat.len;
    store_uint32(&data[pos], flags);
    pos+=4;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

static int send_sftp_stat_v04(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_stat_v04_generic(sftp, sftp_r, valid);
}

/*
    LSTAT
    - byte 1 	SSH_FXP_LSTAT
    - uint32 	request id
    - uint32 	len path (n)
    - byte[n]	path
    - uint32	flags which data of interest
*/

int send_sftp_lstat_v04_generic(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int flags)
{
    char data[17 + sftp_r->call.lstat.len];
    unsigned int pos=0;

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 13 + sftp_r->call.lstat.len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_LSTAT;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.lstat.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.lstat.path, sftp_r->call.lstat.len);
    pos+=sftp_r->call.lstat.len;
    store_uint32(&data[pos], flags);
    pos+=4;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

static int send_sftp_lstat_v04(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_lstat_v04_generic(sftp, sftp_r, valid);
}

/*
    FSTAT
    - byte 1 	SSH_FXP_FSTAT
    - uint32 	request id
    - uint32 	len handle (n)
    - byte[n]	handle
    - uint32	flags which data of interest
*/

int send_sftp_fstat_v04_generic(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int flags)
{
    char data[17 + sftp_r->call.fgetstat.len];
    unsigned int pos=0;

    sftp_r->id=get_sftp_request_id(sftp);

    store_uint32(&data[pos], 13 + sftp_r->call.fgetstat.len);
    pos+=4;
    data[pos]=(unsigned char) SSH_FXP_FSTAT;
    pos++;
    store_uint32(&data[pos], sftp_r->id);
    pos+=4;
    store_uint32(&data[pos], sftp_r->call.fgetstat.len);
    pos+=4;
    memcpy((char *) &data[pos], sftp_r->call.fgetstat.handle, sftp_r->call.fgetstat.len);
    pos+=sftp_r->call.fgetstat.len;
    store_uint32(&data[pos], flags);
    pos+=4;

    return (* sftp_r->send)(sftp_r, data, pos, &sftp_r->reply.sequence, &sftp_r->slist);

}

static int send_sftp_fstat_v04(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r)
{
    unsigned int valid=sftp->attrctx.w_valid.mask | sftp->attrctx.w_valid.flags;
    return send_sftp_fstat_v04_generic(sftp, sftp_r, valid);
}


static struct sftp_send_ops_s send_ops_v04 = {
    .version				= 4,
    .init				= send_sftp_init_v03,
    .open				= send_sftp_open_v03,
    .read				= send_sftp_read_v03,
    .write				= send_sftp_write_v03,
    .close				= send_sftp_close_v03,
    .stat				= send_sftp_stat_v04,
    .lstat				= send_sftp_lstat_v04,
    .fstat				= send_sftp_fstat_v04,
    .setstat				= send_sftp_setstat_v03,
    .fsetstat				= send_sftp_fsetstat_v03,
    .realpath				= send_sftp_realpath_v03,
    .readlink				= send_sftp_readlink_v03,
    .opendir				= send_sftp_opendir_v03,
    .readdir				= send_sftp_readdir_v03,
    .remove				= send_sftp_remove_v03,
    .rmdir				= send_sftp_rmdir_v03,
    .mkdir				= send_sftp_mkdir_v03,
    .rename				= send_sftp_rename_v03,
    .symlink				= send_sftp_symlink_v03,
    .block				= send_sftp_block_v03,
    .unblock				= send_sftp_unblock_v03,
    .extension				= send_sftp_extension_v03,
    .custom				= send_sftp_custom_v03,
};

struct sftp_send_ops_s *get_sftp_send_ops_v04()
{
    return &send_ops_v04;
}

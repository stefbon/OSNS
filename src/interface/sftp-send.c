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
#include "libosns-resources.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp-send.h"

int send_sftp_open_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->open)(sftp, sftp_r);
}

int send_sftp_create_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->create)(sftp, sftp_r);
}

int send_sftp_opendir_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->opendir)(sftp, sftp_r);
}

int send_sftp_read_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->read)(sftp, sftp_r);
}

int send_sftp_write_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->write)(sftp, sftp_r);
}

int send_sftp_readdir_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->readdir)(sftp, sftp_r);
}

int send_sftp_close_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->close)(sftp, sftp_r);
}

int send_sftp_remove_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->remove)(sftp, sftp_r);
}

int send_sftp_rename_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->rename)(sftp, sftp_r);
}

int send_sftp_mkdir_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->mkdir)(sftp, sftp_r);
}

int send_sftp_rmdir_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->rmdir)(sftp, sftp_r);
}

int send_sftp_stat_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->stat)(sftp, sftp_r);
}

int send_sftp_lstat_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->lstat)(sftp, sftp_r);
}

int send_sftp_fstat_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->fstat)(sftp, sftp_r);
}

int send_sftp_setstat_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->setstat)(sftp, sftp_r);
}

int send_sftp_fsetstat_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->fsetstat)(sftp, sftp_r);
}

int send_sftp_readlink_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->readlink)(sftp, sftp_r);
}

int send_sftp_symlink_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->symlink)(sftp, sftp_r);
}

int send_sftp_block_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->block)(sftp, sftp_r);
}

int send_sftp_unblock_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->unblock)(sftp, sftp_r);
}

int send_sftp_realpath_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->realpath)(sftp, sftp_r);
}

int send_sftp_extension_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->extension)(sftp, sftp_r);
}

int send_sftp_custom_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)(* i->get_interface_buffer)(i);
    return (*sftp->send_ops->custom)(sftp, sftp_r);
}


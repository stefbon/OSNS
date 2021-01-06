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
#include <sys/syscall.h>

#define LOGGING
#include "logging.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/attr.h"
#include "sftp-attr.h"

void read_sftp_attributes_ctx(struct context_interface_s *interface, struct attr_response_s *response, struct sftp_attr_s *attr)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    struct attr_buffer_s ab;
    set_attr_buffer_read_attr_response(&ab, response);
    (*sftp->attr_ops.read_attributes)(sftp, &ab, attr);
}
void write_attributes_ctx(struct context_interface_s *interface, char *buffer, unsigned int len, struct sftp_attr_s *attr)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    struct attr_buffer_s ab;
    set_attr_buffer_write(&ab, buffer, len);
    (*sftp->attr_ops.write_attributes)(sftp, &ab, attr);
}
unsigned int write_attributes_len_ctx(struct context_interface_s *interface, struct sftp_attr_s *attr)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    return (*sftp->attr_ops.write_attributes_len)(sftp, attr);
}
void read_name_nameresponse_ctx(struct context_interface_s *interface, struct fuse_buffer_s *buffer, struct ssh_string_s *name)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    struct attr_buffer_s ab;
    set_attr_buffer_read(&ab, buffer->pos, buffer->left);
    (*sftp->attr_ops.read_name_response)(sftp, &ab, name);
    buffer->pos = ab.pos;
    buffer->left = ab.left;
}
void read_attr_nameresponse_ctx(struct context_interface_s *interface, struct fuse_buffer_s *buffer, struct sftp_attr_s *attr)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    struct attr_buffer_s ab;
    set_attr_buffer_read(&ab, buffer->pos, buffer->left);
    (*sftp->attr_ops.read_attr_response)(sftp, &ab, attr);
    buffer->pos = ab.pos;
    buffer->left = ab.left;
    buffer->count++;
}
int get_attribute_info_ctx(struct context_interface_s *interface, unsigned int valid, const char *what)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    return (* sftp->attr_ops.get_attribute_info)(sftp, valid, what);
}
void correct_time_s2c_ctx(struct context_interface_s *interface, struct timespec *t)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    (* sftp->time_ops.correct_time_s2c)(sftp, t);
}
void correct_time_c2s_ctx(struct context_interface_s *interface, struct timespec *t)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    (* sftp->time_ops.correct_time_c2s)(sftp, t);
}

void translate_sftp_attr_fattr(struct context_interface_s *interface)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);

    unsigned int sftpattr=get_sftp_features(sftp);
    unsigned int fuseattr=0;

    if (sftpattr & SFTP_ATTR_PERMISSIONS) fuseattr |= FATTR_MODE;
    if (sftpattr & SFTP_ATTR_SIZE) fuseattr |= FATTR_SIZE;
    if (sftpattr & SFTP_ATTR_ATIME) fuseattr |= FATTR_ATIME;
    if (sftpattr & SFTP_ATTR_MTIME) fuseattr |= FATTR_MTIME;
    if (sftpattr & SFTP_ATTR_CTIME) fuseattr |= FATTR_CTIME;
    if (sftpattr & SFTP_ATTR_USER) fuseattr |= FATTR_UID;
    if (sftpattr & SFTP_ATTR_GROUP) fuseattr |= FATTR_GID;

    interface->backend.sftp.fattr_supported=fuseattr;

}

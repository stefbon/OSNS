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
#include "sftp/extensions.h"
#include "sftp/request-hash.h"

#define SFTP_EXTENSION_NAME_STATVFS			"statvfs@openssh.com"

static struct sftp_protocol_extension_client_s client_extension;

static unsigned int get_data_len_statvfs(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *ext, struct sftp_request_s *sftp_r)
{
    return (4 + sftp_r->call.statvfs.len);
}

static unsigned int fill_data_statvfs(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *ext, struct sftp_request_s *sftp_r, char *data, unsigned int lem)
{
    store_uint32(data, sftp_r->call.statvfs.len);
    data+=4;
    memcpy(data, sftp_r->call.statvfs.path, sftp_r->call.statvfs.len);
    return (sftp_r->call.statvfs.len + 4);
}

static unsigned int get_size_statvfs(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data)
{
    /* extension does not require any additional runtime data */
    return 0;
}

static void init_statvfs(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *ext)
{
    /* does nothing */
}

void register_client_sftp_extension_statvfs(struct sftp_extensions_s *extensions)
{

    init_list_element(&client_extension.list, NULL);
    set_ssh_string(&client_extension.name, 'c', SFTP_EXTENSION_NAME_STATVFS);

    client_extension.get_data_len=get_data_len_statvfs;
    client_extension.fill_data=fill_data_statvfs;

    client_extension.get_size_buffer=get_size_statvfs;
    client_extension.init=init_statvfs;

    add_list_element_last(&extensions->supported_client, &client_extension.list);
}

/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "fuse/browse-fs.h"

#include "osns_client.h"
#include "network.h"
#include "utils.h"
#include "connect.h"
#include "resources.h"
#include "ssh.h"

struct connect_shared_directory_hlpr_s {
    struct service_context_s *dctx;
};

static void process_ssh_host_service(struct service_context_s *sctx, char *data, unsigned int size, void *ptr)
{

    logoutput_debug("process_ssh_host_service: data size %u", size);

    /* process a reply from ssh host when aksing for a service */

    if (data && (size>0)) {
        char *sep=memchr(data, '|', size);

        if (sep) {
            struct connect_shared_directory_hlpr_s *hlpr=(struct connect_shared_directory_hlpr_s *) ptr;
            unsigned char action=0;

            *sep='\0';

            if (create_sftp_filesystem_shared_context(sctx, hlpr->dctx, data, &action)==0) {

                logoutput_debug("process_ssh_host_service: created sftp shared context for %s", data);

            } else {

                logoutput_debug("process_ssh_host_service: unable to create sftp shared context for %s", data);

            }

        }

    }

}

static void connect_shared_directory_cb(struct service_context_s *ctx, struct service_context_s *sctx, void *ptr)
{
    struct context_interface_s *i=&sctx->interface;
    struct interface_status_s istatus;
    struct connect_shared_directory_hlpr_s *hlpr=(struct connect_shared_directory_hlpr_s *) ptr;
    struct service_context_s *dctx=hlpr->dctx;

    logoutput_debug("connect_shared_directory_cb");

    init_interface_status(&istatus);

    if ((* i->get_interface_status)(i, &istatus)>0) {

        if (istatus.flags & (INTERFACE_STATUS_FLAG_ERROR | INTERFACE_STATUS_FLAG_DISCONNECTED)) return;

    }

    /* get the remote service */
    get_remote_service_ssh_host(sctx, "info:service:", dctx->service.filesystem.name, process_ssh_host_service, ptr);
    return;

}

void connect_network_shared_directory(void *ptr)
{
    struct service_context_s *ctx=(struct service_context_s *) ptr;

    if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {
        struct service_context_s *pctx=get_parent_context(ctx); /* must represent a network host */
        struct connect_shared_directory_hlpr_s hlpr={.dctx=ctx};

        do_network_host_cb(pctx, connect_shared_directory_cb, (void *) &hlpr);

    }

}

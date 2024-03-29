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

#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-context.h"
#include "libosns-log.h"

struct service_context_s *get_service_context(struct context_interface_s *interface)
{
    return (struct service_context_s *) ( ((char *) interface) - offsetof(struct service_context_s, interface));
}

void adjust_pathmax(struct service_context_s *ctx, unsigned int len)
{
    signal_set_flag(ctx->service.workspace.signal, &ctx->service.workspace.status, SERVICE_WORKSPACE_FLAG_PATH);
    if (len>ctx->service.workspace.pathmax) ctx->service.workspace.pathmax=len;
    signal_unset_flag(ctx->service.workspace.signal, &ctx->service.workspace.status, SERVICE_WORKSPACE_FLAG_PATH);
}

unsigned int get_pathmax(struct service_context_s *ctx)
{
    return ctx->service.workspace.pathmax;
}

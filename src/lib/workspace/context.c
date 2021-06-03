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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fsuid.h>

#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "threads.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspaces.h"
#include "fuse.h"
#include "context.h"
#include "log.h"

struct service_context_s *get_service_context(struct context_interface_s *interface)
{
    return (struct service_context_s *) ( ((char *) interface) - offsetof(struct service_context_s, interface));
}

struct osns_user_s *get_user_context(struct service_context_s *context)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    return (workspace) ? workspace->user : NULL;
}

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
#include <sys/statfs.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "options.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

int _fs_workspace_access(struct service_context_s *context, struct fuse_request_s *request, unsigned char what)
{
    unsigned int error=0;

    error=EACCES;

    switch (what) {

	case SERVICE_OP_TYPE_LOOKUP:
	case SERVICE_OP_TYPE_LOOKUP_EXISTING:
	case SERVICE_OP_TYPE_LOOKUP_NEW:

	case SERVICE_OP_TYPE_GETATTR:
	case SERVICE_OP_TYPE_READLINK:

	case SERVICE_OP_TYPE_OPENDIR:
	case SERVICE_OP_TYPE_READDIR:
	case SERVICE_OP_TYPE_RELEASEDIR:

	case SERVICE_OP_TYPE_OPEN:
	case SERVICE_OP_TYPE_READ:
	case SERVICE_OP_TYPE_RELEASE:
	case SERVICE_OP_TYPE_FGETATTR:

	case SERVICE_OP_TYPE_GETXATTR:
	case SERVICE_OP_TYPE_LISTXATTR:

	case SERVICE_OP_TYPE_STATFS:
	case SERVICE_OP_TYPE_GETLOCK:

	{

	    error=0;
	    break;

	}

	case SERVICE_OP_TYPE_SETXATTR:
	case SERVICE_OP_TYPE_REMOVEXATTR:

	case SERVICE_OP_TYPE_MKDIR:
	case SERVICE_OP_TYPE_MKNOD:
	case SERVICE_OP_TYPE_SYMLINK:
	case SERVICE_OP_TYPE_SETATTR:

	case SERVICE_OP_TYPE_CREATE:
	case SERVICE_OP_TYPE_WRITE:
	case SERVICE_OP_TYPE_FLUSH:
	case SERVICE_OP_TYPE_FSYNC:
	case SERVICE_OP_TYPE_FSETATTR:
	case SERVICE_OP_TYPE_SETLOCK:
	case SERVICE_OP_TYPE_FLOCK:

	{

	    struct osns_user_s *user=get_user_context(context);

	    /* only user self may do this */

	    error=EPERM;
	    if (user->pwd.pw_uid==request->uid) error=0;


	}



	case SERVICE_OP_TYPE_READDIRPLUS:

	    error=ENOSYS;
	    break;


    }

    return error;

}

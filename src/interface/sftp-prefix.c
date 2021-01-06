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
#include "log.h"

#include "workspace-interface.h"
#include "sftp/common-protocol.h"
#include "sftp/common.h"

/*
    various functions to complete the path of a sftp shared map
    examples:

    - home		: with sftp a path without the starting slash is relative to the homedirectory on the server
    - root		: normal absolute path on the server
    - custom prefix	: a path is set before the path
*/

static int complete_path_sftp_home(struct context_interface_s *interface, char *buffer, struct pathinfo_s *pathinfo)
{
    /* path starts with a slash: ignore that by going one position to the right */
    pathinfo->path++;
    return -1;
}

static int complete_path_sftp_root(struct context_interface_s *interface, char *buffer, struct pathinfo_s *pathinfo)
{
    /* path starts with a slash: leave that intact */
    return 0;
}

static int complete_path_sftp_custom(struct context_interface_s *interface, char *buffer, struct pathinfo_s *pathinfo)
{
    /* custom prefix */
    memcpy(buffer, interface->backend.sftp.prefix.path, interface->backend.sftp.prefix.len);
    memcpy(&buffer[interface->backend.sftp.prefix.len], pathinfo->path, pathinfo->len);
    buffer[interface->backend.sftp.prefix.len + pathinfo->len]='\0';
    pathinfo->path=buffer;
    return interface->backend.sftp.prefix.len;
}

static unsigned int get_complete_pathlen_home(struct context_interface_s *interface, unsigned int len)
{
    return 0;
}

static unsigned int get_complete_pathlen_root(struct context_interface_s *interface, unsigned int len)
{
    return 0;
}

static unsigned int get_complete_pathlen_custom(struct context_interface_s *interface, unsigned int len)
{
    return len + interface->backend.sftp.prefix.len + 1;
}

void set_sftp_interface_prefix(struct context_interface_s *interface, char *name, char *prefix)
{

    if (strcmp(name, "home")==0) {

	interface->backend.sftp.complete_path=complete_path_sftp_home;
	interface->backend.sftp.get_complete_pathlen=get_complete_pathlen_home;
	interface->backend.sftp.prefix.type=INTERFACE_BACKEND_SFTP_PREFIX_HOME;

	if (prefix) {

	    interface->backend.sftp.prefix.path=strdup(prefix);
	    interface->backend.sftp.prefix.len=strlen(prefix);

	} else {

	    interface->backend.sftp.prefix.path=NULL;
	    interface->backend.sftp.prefix.len=0;

	}

    } else if (prefix==NULL || strlen(prefix)==0) {

	interface->backend.sftp.complete_path=complete_path_sftp_root;
	interface->backend.sftp.get_complete_pathlen=get_complete_pathlen_root;
	interface->backend.sftp.prefix.type=INTERFACE_BACKEND_SFTP_PREFIX_ROOT;
	interface->backend.sftp.prefix.path=NULL;
	interface->backend.sftp.prefix.len=0;

    } else {

	/* custom prefix */

	interface->backend.sftp.complete_path=complete_path_sftp_custom;
	interface->backend.sftp.get_complete_pathlen=get_complete_pathlen_custom;
	interface->backend.sftp.prefix.type=INTERFACE_BACKEND_SFTP_PREFIX_CUSTOM;

	if (prefix) {

	    interface->backend.sftp.prefix.path=strdup(prefix);
	    interface->backend.sftp.prefix.len=strlen(prefix);

	} else {

	    interface->backend.sftp.prefix.path=NULL;
	    interface->backend.sftp.prefix.len=0;

	}

    }

}

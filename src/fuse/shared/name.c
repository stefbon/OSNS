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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

unsigned int _fs_shared_get_name(struct service_context_s *ctx, char *buffer, unsigned int len)
{
    char *name=ctx->service.filesystem.name;
    unsigned int size=strlen(name);
    unsigned int pos=0;

    if (compare_starting_substring(name, size, "ssh-channel://session/subsystem/sftp", &pos)==0) {

        /* look for a name, if there is none take "home" (TODO: or remote username) */

        if ((size > pos + 1) && (name[pos]=='/')) {
            char *sep=memrchr(name, '/', size); /* sep will probably == &name[pos] */

            if (sep) {

                name=sep+1;
                size=strlen(name);

            } else {

                /* this should not happen, there is always a slash ... */

                name=NULL;
                size=0;

            }

        } else {

            name="home";
            size=4;

        }

    }

    logoutput_debug("_fs_shared_get_name: found name %s", name);

    if (buffer) {

	if (size<len) len=size;
	if (name) memcpy(buffer, name, len);

    }

    return size;

}


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
#include "libosns-datatypes.h"

#include "fspath.h"

char *fs_path_get_filename(struct fs_path_s *path, struct ssh_string_s *filename)
{
    char *name=NULL;

    if (path->buffer) {
        unsigned int len=path->len;
        char *start=&path->buffer[path->start];

        name=memrchr(start, '/', len);

        if (filename) {

            if (name) {

                filename->ptr=name;
                filename->len=(unsigned int)(start + len - name);

            }

        }

    }

    return name;
}

void fs_path_remove_filename(struct fs_path_s *path, struct ssh_string_s *filename)
{

    if (path->buffer && filename->ptr) {
        char *pstart=(char *)(path->buffer + path->start);
        char *pend=(char *)(pstart + path->len);

        /* only remove filename from path when it's at one of the borders (start or end) */

        if (filename->ptr == pstart) {

            if (filename->ptr < pend) {

                path->start=(unsigned int)(filename->ptr - path->buffer);
                path->len -= filename->len;

            }

        } else if ((char *)(filename->ptr + filename->len) == pend) {

            if (filename->ptr > pstart) {

                path->len -= filename->len;

            }

        }

    }

}


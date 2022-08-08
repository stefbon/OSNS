/*

  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include "handlemime.h"

#ifdef HAVE_GIO2
#include <gio/gio.h>

int open_mimedb()
{
    return 0;
}

void close_mimedb()
{
}

unsigned int get_mimetype(char *path, char *name, unsigned int len)
{
    GFile *file = g_file_new_for_path (path);
    GFileInfo *file_info = g_file_query_info (file, "standard::content-type", 0, NULL, NULL);
    const char *mime = g_file_info_get_content_type (file_info);

    if (mime) {

	strncpy(name, mime, len);
	return len;

    }

    return 0;
}

unsigned char is_mimetype(char *name)
{
    return g_content_type_is_unknown((const gchar *) name);
}

#elif HAVE_LIBMAGIC
#include <magic.h>
static magic_t magic=NULL;

int open_mimedb()
{
    magic=magic_open(MAGIC_MIME_TYPE | MAGIC_PRESERVE_ATIME | MAGIC_NO_CHECK_TAR | MAGIC_NO_CHECK_COMPRESS);

    if (magic) {

	magic_load(magic, NULL);
	return 0;

    }

    return -1;
}

void close_mimedb()
{
    if (magic) {

	magic_close(magic);
	magic=NULL;

    }
}

unsigned int get_mimetype(char *path, char *name, unsigned int len)
{
    if (magic) {
	char *mime = magic_file(magic, path);

	if (mime) {

	    strncpy(name, mime, len);
	    return len;

	}

    }
    return 0;
}

unsigned char is_mimetype(char *name)
{
    return 1;
}

#endif

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
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "path.h"

static void path_append_shared(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct fs_path_s *localpath)
{
    fs_path_append_raw(localpath, "/", 1);

    /* TODO: add convert path from UTF-8 to local encoding */

    fs_path_append_raw(localpath, path->ptr, path->len);

}

void path_append_home(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct fs_path_s *localpath)
{
    fs_path_append_raw(localpath, sftp->identity.home.ptr, sftp->identity.home.len);
    path_append_shared(sftp, path, localpath);
}

void path_append_prefix(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct fs_path_s *localpath)
{
    fs_path_append_raw(localpath, sftp->prefix.path.ptr, sftp->prefix.path.len);
    path_append_shared(sftp, path, localpath);
}

void path_append_none(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct fs_path_s *localpath)
{
    fs_path_append(localpath, 's', path);
}

/* get length of buffer when no prefix is used .. two cases:

    - path is starting with a slash -> no prefix at all
    - path is not starting with a slash -> relative to home directory, so prefix is $HOME of connecting user

    NOTE: no conversion from format used by sftp (is UTF-8 for versions 4-6) to the local format yet */

unsigned int get_length_fullpath_noprefix(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct convert_sftp_path_s *convert)
{
    char *ptr=path->ptr;

    if (path->len>0 && ptr[0]=='/') {

	convert->complete=&path_append_none;
	return path->len;

    }

    convert->complete=&path_append_home;
    return path->len + 1 + sftp->identity.home.len;

}

unsigned int get_length_fullpath_prefix(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct convert_sftp_path_s *convert)
{
    char *ptr=path->ptr;

    if (path->len>0 && ptr[0]=='/') {

	convert->complete=&path_append_prefix;
	return path->len + 1 + sftp->prefix.path.len;

    }

    convert->complete=&path_append_home;
    return path->len + 1 + sftp->identity.home.len;

}

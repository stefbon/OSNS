/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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

#include "libosns-fspath.h"

#include "fssocket.h"

#define SHARED_FSACTION_UNLINK                  1
#define SHARED_FSACTION_RMDIR                   2
#define SHARED_FSACTION_MKDIR                   3

static int system_shared_fsaction(struct fs_path_s *path, unsigned char code, struct fs_init_s *init)
{
    int result=-1;

#ifdef __linux__

    unsigned int size=fs_path_get_length(path);

    if (size>0) {
        char tmp[size + 1];

        size=fs_path_copy(path, tmp, size);
        tmp[size]='\0';

        switch (code) {

            case SHARED_FSACTION_UNLINK:

                result=unlink(tmp);
                break;

            case SHARED_FSACTION_RMDIR:

                result=rmdir(tmp);
                break;

            case SHARED_FSACTION_MKDIR:

                result=mkdir(tmp, init->mode);
                break;

        }

    }

#endif

    return result;

}


int system_remove_file(struct fs_path_s *path)
{
    return system_shared_fsaction(path, SHARED_FSACTION_UNLINK, NULL);
}

int system_remove_dir(struct fs_path_s *path)
{
    return system_shared_fsaction(path, SHARED_FSACTION_RMDIR, NULL);
}

int system_create_dir(struct fs_path_s *path, struct fs_init_s *init)
{
    return system_shared_fsaction(path, SHARED_FSACTION_MKDIR, init);
}

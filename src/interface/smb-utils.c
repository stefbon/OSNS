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
#include <sys/poll.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "users.h"

#include "smb.h"
#include "smb-signal.h"

void get_smb_request_timeout_ctx(struct context_interface_s *interface, struct timespec *timeout)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);

    /* make this configurable?
	depend on earlier timeouts/smb behaviour? */

    timeout->tv_sec=4;
    timeout->tv_nsec=0;

}


void fill_inode_attr_smb(struct context_interface_s *interface, struct stat *st, void *ptr)
{
    struct smb2_stat_64 *smb2_st=(struct smb2_stat_64 *) ptr;
    struct passwd *pwd=get_workspace_user_pwd(interface);

    switch (smb2_st->smb2_type) {

	case SMB2_TYPE_FILE:

	    st->st_mode |= S_IFREG;
	    break;

	case SMB2_TYPE_DIRECTORY:

	    st->st_mode |= S_IFDIR;
	    break;

	case SMB2_TYPE_LINK:

	    st->st_mode |= S_IFLNK;
	    break;

    }

    /* uid and gid: set these from owner of this workspace */

    st->st_uid=pwd->pw_uid;
    st->st_gid=pwd->pw_gid;

    /* set permissions 0666 for files and 0777 for directories */

    if (S_ISDIR(st->st_mode)) {

	st->st_mode |= 0777;

    } else {

	st->st_mode |= 0666;

    }

    st->st_size=smb2_st->smb2_size;

    st->st_atim.tv_sec=smb2_st->smb2_atime;
    st->st_atim.tv_nsec=smb2_st->smb2_atime_nsec;

    st->st_mtim.tv_sec=smb2_st->smb2_mtime;
    st->st_mtim.tv_nsec=smb2_st->smb2_mtime_nsec;

    st->st_ctim.tv_sec=smb2_st->smb2_ctime;
    st->st_ctim.tv_nsec=smb2_st->smb2_ctime_nsec;

    /* ignored:

	st->st_ino	: set by the application
	st->st_nlink
	st->st_dev
	st->st_rdev
    */

}

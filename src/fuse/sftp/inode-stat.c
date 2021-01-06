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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "interface/sftp-attr.h"

typedef void (* copy_attr_cb)(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr);

static void copy_attr_size(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    st->st_size=attr->size;
}

static void copy_attr_mode(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    mode_t mode_keep=(st->st_mode & S_IFMT);
    mode_t perm_keep=st->st_mode - mode_keep;
    st->st_mode=perm_keep | attr->type;
}

static void copy_attr_permissions(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    unsigned int keep=st->st_mode;

    if (attr->received & SFTP_ATTR_TYPE) {

	st->st_mode=attr->permissions | attr->type;

    } else {
	mode_t mode_keep=(st->st_mode & S_IFMT);

	st->st_mode=attr->permissions | mode_keep;

    }

}

static void copy_attr_uid(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    st->st_uid=attr->user.uid;
}

static void copy_attr_gid(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    st->st_gid=attr->group.gid;
}

static void copy_attr_atim(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    st->st_atim.tv_sec=attr->atime;
    st->st_atim.tv_nsec=attr->atime_n;
    correct_time_s2c_ctx(interface, &st->st_atim);
}

static void copy_attr_mtim(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    st->st_mtim.tv_sec=attr->mtime;
    st->st_mtim.tv_nsec=attr->mtime_n;
    correct_time_s2c_ctx(interface, &st->st_mtim);
}

static void copy_attr_ctim(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    st->st_ctim.tv_sec=attr->ctime;
    st->st_ctim.tv_nsec=attr->ctime_n;
    correct_time_s2c_ctx(interface, &st->st_ctim);
}

static void copy_attr_nothing(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
}

static copy_attr_cb copy_attr_acb[][2] = {
	{copy_attr_nothing, copy_attr_size},
	{copy_attr_nothing, copy_attr_uid},
	{copy_attr_nothing, copy_attr_gid},
	{copy_attr_mode, copy_attr_permissions},
	{copy_attr_nothing, copy_attr_atim},
	{copy_attr_nothing, copy_attr_mtim},
	{copy_attr_nothing, copy_attr_ctim}};

/*
    fill the inode values (size,mode,uid,gid,c/m/atime) with the attributes from sftp
    this is not straightforward since it's possible that the server did not provide all values
    param:
    - ptr				pointer to sftp context
    - inode				inode to fill
    - fuse_sftp_attr_s			values received from server
*/

void fill_inode_attr_sftp(struct context_interface_s *interface, struct stat *st, struct sftp_attr_s *attr)
{
    unsigned char vb[7];

    vb[0] = (attr->received && SFTP_ATTR_SIZE);
    vb[1] = (attr->received && SFTP_ATTR_USER);
    vb[2] = (attr->received && SFTP_ATTR_GROUP);
    vb[3] = (attr->received && SFTP_ATTR_PERMISSIONS);
    vb[4] = (attr->received && SFTP_ATTR_ATIME);
    vb[5] = (attr->received && SFTP_ATTR_MTIME);
    vb[6] = (attr->received && SFTP_ATTR_CTIME);

    /* size */

    (* copy_attr_acb[0][vb[0]])(interface, st, attr);

    /* owner */

    (* copy_attr_acb[1][vb[1]])(interface, st, attr);

    /* group */

    (* copy_attr_acb[2][vb[2]])(interface, st, attr);

    /* permissions */

    (* copy_attr_acb[3][vb[3]])(interface, st, attr);

    /* atime */

    (* copy_attr_acb[4][vb[4]])(interface, st, attr);

    /* mtime */

    (* copy_attr_acb[5][vb[5]])(interface, st, attr);

    /* ctime */

    (* copy_attr_acb[6][vb[6]])(interface, st, attr);

}

/*
    translate the attributes to set from fuse to a buffer sftp understands

    FUSE (20161123) :

    FATTR_MODE
    FATTR_UID
    FATTR_GID
    FATTR_SIZE
    FATTR_ATIME
    FATTR_MTIME
    FATTR_FH
    FATTR_ATIME_NOW
    FATTR_MTIME_NOW
    FATTR_LOCKOWNER
    FATTR_CTIME

    to

    SFTP:

    - size
    - owner
    - group
    - permissions
    - access time
    - modify time
    - change time

    (there are more attributes in sftp, but those are not relevant)

    TODO:
    find out about lock owner

*/

unsigned int get_attr_buffer_size(struct context_interface_s *interface, struct stat *st, unsigned int fattr, struct sftp_attr_s *attr, unsigned char raw)
{
    unsigned int set=0;

    memset(attr, 0, sizeof(struct sftp_attr_s));

    set=(raw==0) ? (fattr & interface->backend.sftp.fattr_supported) : fattr;

    if (set & FATTR_SIZE) {

	logoutput("get_attr_buffer_size: set size: %lu", st->st_size);

	attr->asked |= SFTP_ATTR_SIZE;
	attr->size=st->st_size;

    }

    if (set & FATTR_UID) {

	logoutput("get_attr_buffer_size: set owner: %i", (unsigned int) st->st_uid);

	attr->asked |= SFTP_ATTR_USER;
	attr->user.uid=st->st_uid;

    }

    if (set & FATTR_GID) {

	logoutput("get_attr_buffer_size: set group: %i", (unsigned int) st->st_gid);

	attr->asked |= SFTP_ATTR_GROUP;
	attr->group.gid=st->st_gid;

    }

    if (set & FATTR_MODE) {

	attr->asked |= SFTP_ATTR_PERMISSIONS;
	attr->permissions=(st->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));

	logoutput("get_attr_buffer_size: set permissions: %i", (unsigned int) attr->permissions);

    }

    if (set & FATTR_ATIME) {
	struct timespec time;

	logoutput("get_attr_buffer_size: set atime");

	if (set & FATTR_ATIME_NOW) get_current_time(&st->st_atim);

	time.tv_sec=st->st_atim.tv_sec;
	time.tv_nsec=st->st_atim.tv_nsec;

	correct_time_c2s_ctx(interface, &time);

	attr->atime=time.tv_sec;
	attr->atime_n=time.tv_nsec;
	attr->asked |= SFTP_ATTR_ATIME;

    }

    if (set & FATTR_MTIME) {
	struct timespec time;

	if (set & FATTR_MTIME_NOW) get_current_time(&st->st_mtim);

	time.tv_sec=st->st_mtim.tv_sec;
	time.tv_nsec=st->st_mtim.tv_nsec;

	correct_time_c2s_ctx(interface, &time);

	attr->mtime=time.tv_sec;
	attr->mtime_n=time.tv_nsec;
	attr->asked |= SFTP_ATTR_MTIME;

    }

    if (set & FATTR_CTIME) {
	struct timespec time;

	time.tv_sec=st->st_ctim.tv_sec;
	time.tv_nsec=st->st_ctim.tv_nsec;

	correct_time_c2s_ctx(interface, &time);

	attr->ctime=time.tv_sec;
	attr->ctime_n=time.tv_nsec;
	attr->asked |= SFTP_ATTR_CTIME;

    }

    attr->type=(st->st_mode & S_IFMT);
    return write_attributes_len_ctx(interface, attr);

}

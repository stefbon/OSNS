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
#include <sys/vfs.h>
#include <pwd.h>

#include "main.h"
#include "logging.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "osns_sftp_subsystem.h"
#include "protocol.h"
#include "ownergroup.h"

static unsigned int type_mapping[10]={0, S_IFREG, S_IFDIR, S_IFLNK, 0, 0, S_IFSOCK, S_IFCHR, S_IFBLK, S_IFIFO};

typedef unsigned int (* read_attr_cb)(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr);

static unsigned int read_attr_zero(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    /* does nothing*/
    return 0;
}

static unsigned int read_attr_size(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{

    attr->size=get_uint64(buffer);
    attr->valid[SFTP_ATTR_INDEX_SIZE]=1;
    attr->count++;

    logoutput("read_attr_size: size=%li", attr->size);

    return 8; /* 64 bits takes 8 bytes */
}

/* translate owner and group in uid and gid */

static unsigned int read_attr_ownergroup(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    char *pos=buffer;

    attr->user.len=get_uint32(pos);
    pos+=4;

    logoutput("read_attr_ownergroup: user len %i", attr->user.len);

    if (attr->user.len>0) {

	attr->user.ptr=pos;
	pos+=attr->user.len;
	attr->uid=read_sftp_owner(&attr->user, &attr->flags);

    } else {

	attr->uid=unknown_sftp_owner();
	attr->flags|=SFTP_ATTR_FLAG_NOUSER;

    }

    attr->group.len=get_uint32(pos);
    pos+=4;

    logoutput("read_attr_ownergroup: group len %i", attr->group.len);

    if (attr->group.len>0) {

	attr->group.ptr=pos;
	pos+=attr->group.len;
	attr->gid=read_sftp_group(&attr->group, &attr->flags);

    } else {

	attr->gid=unknown_sftp_group();
	attr->flags|=SFTP_ATTR_FLAG_NOGROUP;

    }

    attr->valid[SFTP_ATTR_INDEX_USERGROUP]=1;
    attr->count++;

    return 8 + attr->user.len + attr->group.len;
}

static unsigned int read_attr_allocation_size(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("read_attr_allocation_size");
    return 8; /* 64 bits takes 8 bytes */
}

static unsigned int read_attr_permissions(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->permissions=(S_IRWXU | S_IRWXG | S_IRWXO) & get_uint32(buffer);
    attr->valid[SFTP_ATTR_INDEX_PERMISSIONS]=1;
    attr->count++;
    logoutput("read_attr_permissions: perm %i", attr->permissions);
    return 4;
}

static unsigned int read_attr_accesstime(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->atime.sec=get_int64(buffer);
    attr->valid[SFTP_ATTR_INDEX_ATIME]=1;
    attr->count++;
    logoutput("read_attr_atime: atime.sec=%lli", attr->atime.sec);
    return 8;
}

static unsigned int read_attr_accesstime_n(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->atime.nsec=get_uint32(buffer);
    attr->valid[SFTP_ATTR_INDEX_ATIME_N]=1;
    attr->count++;
    logoutput("read_attr_atime_n: atime.nsec=%i", attr->atime.nsec);
    return 4;
}

static unsigned int read_attr_createtime(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("read_attr_createtime");
    return 8;
}

static unsigned int read_attr_createtime_n(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("read_attr_createtime_n");
    return 4;
}

static unsigned int read_attr_modifytime(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->mtime.sec=get_int64(buffer);
    attr->valid[SFTP_ATTR_INDEX_MTIME]=1;
    attr->count++;
    logoutput("read_attr_mtime: mtime.sec=%lli", attr->mtime.sec);
    return 8;
}

static unsigned int read_attr_modifytime_n(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->mtime.nsec=get_uint32(buffer);
    attr->valid[SFTP_ATTR_INDEX_MTIME_N]=1;
    attr->count++;
    logoutput("read_attr_mtime_n: mtime.nsec=%i", attr->mtime.nsec);
    return 4;
}

static unsigned int read_attr_changetime(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->ctime.sec=get_int64(buffer);
    attr->valid[SFTP_ATTR_INDEX_CTIME]=1;
    attr->count++;
    logoutput("read_attr_ctime: ctime.sec=%lli", attr->ctime.sec);
    return 8;
}

static unsigned int read_attr_changetime_n(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    attr->ctime.nsec=get_uint32(buffer);
    attr->valid[SFTP_ATTR_INDEX_CTIME_N]=1;
    attr->count++;
    logoutput("read_attr_ctime_n: ctime.sec=%i", attr->ctime.nsec);
    return 4;
}

static read_attr_cb read_attr_acb[][2] = {
	{read_attr_zero, read_attr_size},
	{read_attr_zero, read_attr_allocation_size},
	{read_attr_zero, read_attr_ownergroup},
	{read_attr_zero, read_attr_permissions},
	{read_attr_zero, read_attr_accesstime},
	{read_attr_zero, read_attr_accesstime_n},
	{read_attr_zero, read_attr_createtime},
	{read_attr_zero, read_attr_createtime_n},
	{read_attr_zero, read_attr_modifytime},
	{read_attr_zero, read_attr_modifytime_n},
	{read_attr_zero, read_attr_changetime},
	{read_attr_zero, read_attr_changetime_n}};

static unsigned int read_sftp_attributes(struct sftp_subsystem_s *s, unsigned int valid, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    unsigned char vb[12];
    char *pos=buffer;
    unsigned char type=0;
    unsigned char subseconds=0;

    /* translate the valid parameter into a set of bits */

    vb[0]=(valid & SSH_FILEXFER_ATTR_SIZE) >> SSH_FILEXFER_INDEX_SIZE;
    vb[1]=(valid & SSH_FILEXFER_ATTR_ALLOCATION_SIZE) >> SSH_FILEXFER_INDEX_ALLOCATION_SIZE;
    vb[2]=(valid & SSH_FILEXFER_ATTR_OWNERGROUP) >> SSH_FILEXFER_INDEX_OWNERGROUP;
    vb[3]=(valid & SSH_FILEXFER_ATTR_PERMISSIONS) >> SSH_FILEXFER_INDEX_PERMISSIONS;
    vb[4]=(valid & SSH_FILEXFER_ATTR_ACCESSTIME) >> SSH_FILEXFER_INDEX_ACCESSTIME;
    vb[6]=(valid & SSH_FILEXFER_ATTR_CREATETIME) >> SSH_FILEXFER_INDEX_CREATETIME;
    vb[8]=(valid & SSH_FILEXFER_ATTR_MODIFYTIME) >> SSH_FILEXFER_INDEX_MODIFYTIME;
    vb[10]=(valid & SSH_FILEXFER_ATTR_CTIME) >> SSH_FILEXFER_INDEX_CTIME;

    subseconds=(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) >> SSH_FILEXFER_INDEX_SUBSECOND_TIMES;

    /* read nseconds only if time and subseconds_times bits both are set */

    vb[5]=subseconds & vb[4];
    vb[7]=subseconds & vb[6];
    vb[9]=subseconds & vb[8];
    vb[11]=subseconds & vb[10];

    /*	read type (always present)
	- byte			type	*/

    type=(unsigned char) *pos;

    if (type<10) {

	attr->type=type_mapping[type];

    } else {

	attr->type=0;

    }

    logoutput("read_sftp_attributes: type %i (sftp type %i)", attr->type, type);
    pos++;

    /*	size	*/

    pos += (* read_attr_acb[0][vb[0]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	allocation size	*/

    pos += (* read_attr_acb[1][vb[1]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	owner and group */

    pos += (* read_attr_acb[2][vb[2]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	permissions	*/

    pos += (* read_attr_acb[3][vb[3]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	accesstime	*/

    pos += (* read_attr_acb[4][vb[4]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	accesstime_n	*/

    pos += (* read_attr_acb[5][vb[5]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	createtime	*/

    pos += (* read_attr_acb[6][vb[6]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	createtime_n	*/

    pos += (* read_attr_acb[7][vb[7]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	modifytime	*/

    pos += (* read_attr_acb[8][vb[8]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	modifytime_n	*/

    pos += (* read_attr_acb[9][vb[9]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	changetime	*/

    pos += (* read_attr_acb[10][vb[10]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /*	changetime_n	*/

    pos += (* read_attr_acb[11][vb[11]])(s, pos, (unsigned int)(buffer + size - pos + 1), attr);
    logoutput("read_sftp_attributes: %i", attr->count);
    return attr->count;

}

/* read attributes from a buffer as send by the client */

unsigned int read_attributes_v06(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    char *pos=buffer;
    unsigned int valid=0;

    valid=get_uint32(pos);
    pos+=4;
    logoutput("read_attributes_v06: valid %i", valid);

    return read_sftp_attributes(s, valid, pos, size-4, attr);

}

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

static unsigned int type_reverse[13]={SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_FIFO, SSH_FILEXFER_TYPE_CHAR_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_DIRECTORY, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_BLOCK_DEVICE, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_REGULAR, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SYMLINK, SSH_FILEXFER_TYPE_UNKNOWN, SSH_FILEXFER_TYPE_SOCKET};

typedef unsigned int (* write_attr_cb)(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr);
typedef unsigned int (* write_attr_cbl)(struct sftp_subsystem_s *s, struct sftp_attr_s *attr);

/* functions to write attributes (stat values) into sftp format */

static unsigned int write_attr_zero(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    return 0;
}

static unsigned int write_attr_zerol(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 0;
}

static unsigned int write_attr_size(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    store_uint64(pos, attr->size);
    logoutput("write_attr_size");
    return 8;
}

static unsigned int write_attr_size_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 8;
}

static unsigned int write_attr_ownergroup(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr)
{
    char *pos=buffer;

    store_uint32(pos, attr->user.len);
    pos+=4;
    memcpy(pos, attr->user.ptr, attr->user.len);
    pos+=attr->user.len;

    store_uint32(pos, attr->group.len);
    pos+=4;
    memcpy(pos, attr->group.ptr, attr->group.len);
    pos+=attr->group.len;

    logoutput("write_attr_ownergroup");

    return (unsigned int) (pos-buffer);

}

static unsigned int write_attr_ownergroup_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    unsigned int result=0;

    if (get_username(attr->uid, &attr->user)>0 && get_groupname(attr->gid, &attr->group)>0) result=(8 + attr->user.len + attr->group.len);

    logoutput("write_attr_ownergroup_len: username %.*s", attr->user.len, attr->user.ptr);
    logoutput("write_attr_ownergroup_len: groupname %.*s", attr->group.len, attr->group.ptr);

    return 0;
}

static unsigned int write_attr_permissions(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    store_uint32(pos, attr->permissions & ( S_IRWXU | S_IRWXG | S_IRWXO ));
    logoutput("write_attr_permissions");
    return 4;
}

static unsigned int write_attr_permissions_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 4;
}

static unsigned int write_attr_time(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_time_s *time)
{
    store_uint64(pos, time->sec);
    return 8;
}

static unsigned int write_attr_time_n(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_time_s *time)
{
    store_uint32(pos, time->nsec);
    return 4;
}

static unsigned int write_attr_accesstime(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("write_attr_accesstime");
    return write_attr_time(s, pos, size, &attr->atime);
}

static unsigned int write_attr_accesstime_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 8;
}

static unsigned int write_attr_accesstime_n(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("write_attr_accesstime_n");
    return write_attr_time_n(s, pos, size, &attr->atime);
}

static unsigned int write_attr_accesstime_n_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 4;
}

static unsigned int write_attr_modifytime(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("write_attr_modifytime");
    return write_attr_time(s, pos, size, &attr->mtime);
}

static unsigned int write_attr_modifytime_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 8;
}

static unsigned int write_attr_modifytime_n(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("write_attr_modifytime_n");
    return write_attr_time_n(s, pos, size, &attr->mtime);
}

static unsigned int write_attr_modifytime_n_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 4;
}

static unsigned int write_attr_changetime(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("write_attr_changetime");
    return write_attr_time(s, pos, size, &attr->ctime);
}

static unsigned int write_attr_changetime_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 8;
}

static unsigned int write_attr_changetime_n(struct sftp_subsystem_s *s, char *pos, unsigned int size, struct sftp_attr_s *attr)
{
    logoutput("write_attr_changetime_n");
    return write_attr_time_n(s, pos, size, &attr->ctime);
}

static unsigned int write_attr_changetime_n_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr)
{
    return 4;
}

static write_attr_cb write_attr_acb[][2] = {
	{write_attr_zero, write_attr_size},
	{write_attr_zero, write_attr_ownergroup},
	{write_attr_zero, write_attr_permissions},
	{write_attr_zero, write_attr_accesstime},
	{write_attr_zero, write_attr_accesstime_n},
	{write_attr_zero, write_attr_modifytime},
	{write_attr_zero, write_attr_modifytime_n},
	{write_attr_zero, write_attr_changetime},
	{write_attr_zero, write_attr_changetime_n}};

static write_attr_cbl write_attr_acbl[][2] = {
	{write_attr_zerol, write_attr_size_len},
	{write_attr_zerol, write_attr_ownergroup_len},
	{write_attr_zerol, write_attr_permissions_len},
	{write_attr_zerol, write_attr_accesstime_len},
	{write_attr_zerol, write_attr_accesstime_n_len},
	{write_attr_zerol, write_attr_modifytime_len},
	{write_attr_zerol, write_attr_modifytime_n_len},
	{write_attr_zerol, write_attr_changetime_len},
	{write_attr_zerol, write_attr_changetime_n_len}};


unsigned int write_attributes_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr, struct stat *st, unsigned int valid)
{
    unsigned int len=0;
    unsigned char subseconds=0;

    attr->valid[0]=(valid & SSH_FILEXFER_ATTR_SIZE);
    attr->valid[1]=(valid & SSH_FILEXFER_ATTR_OWNERGROUP) >> SSH_FILEXFER_INDEX_OWNERGROUP;
    attr->valid[2]=(valid & SSH_FILEXFER_ATTR_PERMISSIONS) >> SSH_FILEXFER_INDEX_PERMISSIONS;
    attr->valid[3]=(valid & SSH_FILEXFER_ATTR_ACCESSTIME) >> SSH_FILEXFER_INDEX_ACCESSTIME;
    attr->valid[5]=(valid & SSH_FILEXFER_ATTR_MODIFYTIME) >> SSH_FILEXFER_INDEX_MODIFYTIME;
    attr->valid[7]=(valid & SSH_FILEXFER_ATTR_CTIME) >> SSH_FILEXFER_INDEX_CTIME;

    subseconds=(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) >> SSH_FILEXFER_INDEX_SUBSECOND_TIMES;

    attr->valid[4]=subseconds & attr->valid[3];
    attr->valid[6]=subseconds & attr->valid[5];
    attr->valid[8]=subseconds & attr->valid[7];

    attr->type=IFTODT(st->st_mode);
    attr->size=(uint64_t) st->st_size;
    attr->uid=st->st_uid;
    attr->user.len=0;
    attr->user.ptr=NULL;
    attr->gid=st->st_gid;
    attr->group.len=0;
    attr->group.ptr=NULL;
    attr->permissions=st->st_mode & ( S_IRWXU | S_IRWXG | S_IRWXO );
    attr->atime.sec=st->st_atim.tv_sec;
    attr->atime.nsec=st->st_atim.tv_nsec;
    attr->mtime.sec=st->st_mtim.tv_sec;
    attr->mtime.nsec=st->st_mtim.tv_nsec;
    attr->ctime.sec=st->st_ctim.tv_sec;
    attr->ctime.nsec=st->st_ctim.tv_nsec;

    len=5; /* valid flag + type byte */

    /* size */

    len+= (* write_attr_acbl[0][attr->valid[0]]) (s, attr);

    /* owner and/or group */

    len+= (* write_attr_acbl[1][attr->valid[1]]) (s, attr);

    /* permissions */

    len+= (* write_attr_acbl[2][attr->valid[2]]) (s, attr);

    /* access time */

    len+= (* write_attr_acbl[3][attr->valid[3]]) (s, attr);

    /* access time nsec */

    len+= (* write_attr_acbl[4][attr->valid[4]]) (s, attr);

    /* modify time */

    len+= (* write_attr_acbl[5][attr->valid[5]]) (s, attr);

    /* modify time nsec */

    len+= (* write_attr_acbl[6][attr->valid[6]]) (s, attr);

    /* change time */

    len+= (* write_attr_acbl[7][attr->valid[7]]) (s, attr);

    /* change time nsec */

    len+= (* write_attr_acbl[8][attr->valid[8]]) (s, attr);

    // logoutput("write_attributes_len: len %i", len);

    return len;

}

unsigned int write_attributes(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr, unsigned int valid)
{
    unsigned int len=0;
    char *pos=buffer;

    store_uint32(pos, valid);
    pos+=4;

    logoutput("write_attributes: attr->type %i valid %i", attr->type, valid);

    if (attr->type > 13) {

	*pos=(unsigned char) SSH_FILEXFER_TYPE_UNKNOWN;

    } else {

	*pos=type_reverse[attr->type];

    }

    pos++;

    /* size */

    pos+= (* write_attr_acb[0][attr->valid[0]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* owner and/or group */

    pos+= (* write_attr_acb[1][attr->valid[1]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* permissions */

    pos+= (* write_attr_acb[2][attr->valid[2]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* access time */

    pos+= (* write_attr_acb[3][attr->valid[3]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* access time n*/

    pos+= (* write_attr_acb[4][attr->valid[4]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* modify time */

    pos+= (* write_attr_acb[5][attr->valid[5]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* modify time n*/

    pos+= (* write_attr_acb[6][attr->valid[6]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* change time */

    pos+= (* write_attr_acb[7][attr->valid[7]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    /* change time n*/

    pos+= (* write_attr_acb[8][attr->valid[8]]) (s, pos, (unsigned int)(buffer + size - pos + 1), attr);

    if (attr->user.ptr) {

	free(attr->user.ptr);
	attr->user.ptr=NULL;

    }

    if (attr->group.ptr) {

	free(attr->group.ptr);
	attr->group.ptr=NULL;

    }

    // logoutput("write_attributes: len %i", (unsigned int) (pos - buffer));

    return (unsigned int) (pos - buffer);

}

unsigned int write_readdir_attr(struct sftp_subsystem_s *s, char *buffer, unsigned int size, char *name, unsigned int lenname, struct stat *st, unsigned int valid, unsigned int *error)
{
    struct sftp_attr_s attr;
    unsigned int attrlen=write_attributes_len(s, &attr, st, valid);

    /* name to append looks like:
	string			filename (len = 4 + strlen(name))
	ATTRS			attrs (5)
    */

    if (4 + lenname + attrlen <= size) {
	unsigned int pos=0;

	store_uint32(&buffer[pos], lenname);
	pos+=4;
	memcpy(&buffer[pos], name, lenname);
	pos+=lenname;

	pos += write_attributes(s, &buffer[pos], size-pos, &attr, valid);
	logoutput("write_readdir_attr: name %.*s valid %i size %i", lenname, name, valid, pos);
	return pos;

    }

    *error=ENOBUFS;
    return 0;

}

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

#include "log.h"

#include "main.h"
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "mountinfo.h"

#include "misc.h"
#include "osns_sftp_subsystem.h"
#include "protocol.h"

int send_sftp_subsystem(struct sftp_subsystem_s *sftp, char *data, unsigned int len)
{
    struct sftp_connection_s *c=&sftp->connection;
    return (* c->write)(c, data, len);
}

int reply_sftp_status_simple(struct sftp_subsystem_s *sftp, uint32_t id, unsigned int status)
{
    char data[21];
    unsigned int pos=4;

    data[pos]=SSH_FXP_STATUS;
    pos++;
    store_uint32(&data[pos], id);
    pos+=4;
    store_uint32(&data[pos], status);
    pos+=4;
    store_uint32(&data[pos], 0);
    pos+=4;
    store_uint32(&data[pos], 0);
    pos+=4;

    store_uint32(&data[0], pos-4);
    return send_sftp_subsystem(sftp, data, pos);
}


int reply_sftp_bytes_common(struct sftp_subsystem_s *sftp, uint32_t id, unsigned char code, char *bytes, unsigned int len)
{
    char data[13+len];
    unsigned int pos=4;

    data[pos]=code;
    pos++;
    store_uint32(&data[pos], id);
    pos+=4;
    store_uint32(&data[pos], len);
    pos+=4;
    memcpy(&data[pos], bytes, len);
    pos+=len;

    store_uint32(&data[0], pos-4);
    return send_sftp_subsystem(sftp, data, pos);
}

/* same as reply_sftp_bytes_common, but then without the indicator of the length before the data/bytes/attr */

int reply_sftp_attrs(struct sftp_subsystem_s *sftp, uint32_t id, char *attr, unsigned int len)
{
    char data[9+len];
    unsigned int pos=4;

    data[pos]=SSH_FXP_ATTRS;
    pos++;
    store_uint32(&data[pos], id);
    pos+=4;
    memcpy(&data[pos], attr, len);
    pos+=len;

    store_uint32(&data[0], pos-4);
    return send_sftp_subsystem(sftp, data, pos);
}

int reply_sftp_data(struct sftp_subsystem_s *sftp, uint32_t id, char *bytes, unsigned int len, unsigned char eof)
{
    char data[14+len];
    unsigned int pos=4;

    data[pos]=SSH_FXP_DATA;
    pos++;
    store_uint32(&data[pos], id);
    pos+=4;
    store_uint32(&data[pos], len);
    pos+=4;
    memcpy(&data[pos], bytes, len);
    pos+=len;
    if (eof) {

	data[pos]=1;
	pos++;

    }

    store_uint32(&data[0], pos-4);
    return send_sftp_subsystem(sftp, data, pos);
}

int reply_sftp_handle(struct sftp_subsystem_s *sftp, uint32_t id, char *handle, unsigned int len)
{
    return reply_sftp_bytes_common(sftp, id, SSH_FXP_HANDLE, handle, len);
}

int reply_sftp_names(struct sftp_subsystem_s *sftp, uint32_t id, unsigned int count, char *names, unsigned int len, unsigned char eof)
{
    char data[14 + len];
    unsigned int pos=4;

    data[pos]=SSH_FXP_NAME;
    pos++;
    store_uint32(&data[pos], id);
    pos+=4;
    store_uint32(&data[pos], count);
    pos+=4;
    memcpy(&data[pos], names, len);
    pos+=len;
    if (eof) {

	data[pos]=1;
	pos++;

    }

    store_uint32(&data[0], pos-4);
    return send_sftp_subsystem(sftp, data, pos);
}

int reply_sftp_extension(struct sftp_subsystem_s *sftp, uint32_t id, char *data, unsigned int len)
{
    return reply_sftp_bytes_common(sftp, id, SSH_FXP_EXTENDED_REPLY, data, len);
}

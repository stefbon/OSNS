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
#include <sys/statvfs.h>
#include <sys/mount.h>

#include "log.h"

#include "main.h"
#include "misc.h"
#include "list.h"
#include "options.h"
#include "datatypes.h"
#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "lib/workspace/session.h"

#include "osns_socket.h"

extern struct fs_options_s fs_options;

static int write_osns_socket(struct osns_localsocket_s *localsocket, char *buff, unsigned int size, unsigned int *error)
{
    struct socket_ops_s *sops=localsocket->connection.io.socket.sops;
    ssize_t written=0;
    char *pos=buff;
    int left=(int) size;

    writesocket:

    logoutput_debug("write_osns_socket: left %i", left);

    written=(* sops->send)(&localsocket->connection.io.socket, pos, left, 0);

    if (written==-1) {

	if (errno==EAGAIN || errno==EWOULDBLOCK) goto writesocket;
	*error=errno;
	return -e;

    }

    pos += written;
    left -= written;
    if (left>0) goto writesocket;

    return 0;
}

int send_osns_packet(struct osns_localsocket_s *localsocket, char *buff, unsigned int size, unsigned int *error)
{
    int result=0;

    pthread_mutex_lock(&localsocket->mutex);
    result=write_osns_socket(localsocket, buff, size, error);
    pthread_mutex_unlock(&localsocket->mutex);

    return result;
}

int send_osns_msg_notsupported(struct osns_localsocket_s *localsocket, uint32_t id)
{
    char data[9];
    unsigned int error=0;
    unsigned int pos=0;

    store_uint32(&data[pos], 5);
    pos+=4;
    data[pos]=OSNS_MSG_NOTSUPPORTED;
    pos++;
    store_uint32(&data[pos], id);
    pos+=4;

    return send_osns_packet(localsocket, data, pos, &error);
}

int send_osns_msg_init(struct osns_localsocket_s *localsocket, unsigned int major, unsigned int minor)
{
    char data[13];
    unsigned int error=0;
    unsigned int pos=0;

    store_uint32(&data[pos], 8);
    pos+=4;
    data[pos]=OSNS_MSG_VERSION;
    pos++;
    store_uint32(&data[pos], major);
    pos+=4;
    store_uint32(&data[pos], minor);
    pos+=4;

    return send_osns_packet(localsocket, data, pos, &error);
}


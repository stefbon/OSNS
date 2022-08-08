/*
  2017 Stef Bon <stefbon@gmail.com>

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
#include "utils.h"

/* initialize a message to send an fd over a unix socket 
    copied from question on stackoverflow:
    https://stackoverflow.com/questions/37885831/ubuntu-linux-send-file-descriptor-with-unix-domain-socket#37885976
*/

unsigned int get_msghdr_controllen(struct msghdr *message, const char *what)
{

    if (strcmp(what, "int")==0) {

	return CMSG_SPACE(sizeof(int));

    }

    return 0;
}

void add_fd_msghdr(struct msghdr *message, char *buffer, unsigned int size, int fd)
{
    struct cmsghdr *ctrl_message = NULL;

    /* init space ancillary data */

    memset(buffer, 0, size);
    message->msg_control = buffer;
    message->msg_controllen = size;

    /* assign fd to a single ancillary data element */

    ctrl_message = CMSG_FIRSTHDR(message);
    ctrl_message->cmsg_level = SOL_SOCKET;
    ctrl_message->cmsg_type = SCM_RIGHTS;
    ctrl_message->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *) CMSG_DATA(ctrl_message)) = fd;

}

/* read fd from a message
    copied from question on stackoverflow:
    https://stackoverflow.com/questions/37885831/ubuntu-linux-send-file-descriptor-with-unix-domain-socket#37885976
*/

int read_fd_msghdr(struct msghdr *message)
{
    struct cmsghdr *ctrl_message = NULL;
    int fd=-1;

    /* iterate ancillary elements */

    for (ctrl_message = CMSG_FIRSTHDR(message); ctrl_message != NULL; ctrl_message = CMSG_NXTHDR(message, ctrl_message)) {

	if ((ctrl_message->cmsg_level == SOL_SOCKET) && (ctrl_message->cmsg_type == SCM_RIGHTS)) {

	    fd = *((int *) CMSG_DATA(ctrl_message));
	    break;

	}

    }

    return fd;

}


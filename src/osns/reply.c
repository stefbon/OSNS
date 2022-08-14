/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-socket.h"

#include "osns-protocol.h"

#include "receive.h"
#include "reply.h"
#include "control.h"
#include "utils.h"

/* send a version message as reply to the init message send by client
    20220131: only support for version 1
*/

int osns_reply_init(struct osns_receive_s *r, unsigned int version, unsigned int services)
{
    unsigned int versionmajor=get_osns_major(version);

    if (versionmajor==1) {
	char data[13];
	unsigned int pos=0;

	store_uint32(&data[pos], 9);
	pos+=4;
	data[pos]=OSNS_MSG_VERSION;
	pos++;
	store_uint32(&data[pos], version);
	pos+=4;
	store_uint32(&data[pos], services);
	pos+=4;

	return (* r->send)(r, data, pos, NULL, NULL);

    }

    return -1;
}

int osns_reply_status(struct osns_receive_s *r, uint32_t id, unsigned int status, char *extra, unsigned int len)
{
    char data[17 + len];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_STATUS;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    store_uint32(&data[pos], status);
    pos+=4;

    if (extra && len>0) {

	store_uint32(&data[pos], len);
	pos+=4;
	memcpy(&data[pos], extra, len);
	pos+=len;

    }

    store_uint32(&data[0], pos-4);
    return (* r->send)(r, data, pos, NULL, NULL);
}

int osns_reply_name(struct osns_receive_s *r, uint32_t id, struct name_string_s *name)
{
    char data[13 + name->len];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_NAME;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    /* name length is in 1 byte */

    logoutput("osns_reply_name: len %i", name->len);

    data[pos]=(unsigned char) (name->len);
    pos++;
    memcpy(&data[pos], name->ptr, name->len);
    pos+=name->len;

    store_uint32(&data[0], pos-4);
    return (* r->send)(r, data, pos, NULL, NULL);
}

int osns_reply_records(struct osns_receive_s *r, uint32_t id, unsigned int count, char *records, unsigned int len)
{
    char data[17 + len];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_RECORDS;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    store_uint32(&data[pos], count);
    pos+=4;

    memcpy(&data[pos], records, len);
    pos+=len;

    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, NULL, NULL);
}

static int send_data_cb_socket(struct osns_socket_s *sock, char *data, unsigned int size, void *ptr)
{
    struct iovec iov[1];
    struct msghdr msg;
    struct osns_socket_s *tosend=(struct osns_socket_s *) ptr;

#ifdef __linux__

    int fd=(* tosend->get_unix_fd)(tosend);
    unsigned int tmp=sizeof(int);
    union {
	char 			buffer[CMSG_SPACE(tmp)];
	struct cmsghdr		allign;
    } uhlp;
    struct cmsghdr *cmsg=NULL;

    memset(&uhlp, 0, sizeof(uhlp));

#endif

    iov[0].iov_base=(void *) data;
    iov[0].iov_len=(size_t) size;

    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;
    msg.msg_flags=0;

#ifdef __linux__

    msg.msg_control=(void *) uhlp.buffer;
    msg.msg_controllen=sizeof(uhlp.buffer);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(tmp);

    memcpy(CMSG_DATA(cmsg), &fd, tmp);

#else

    msg.msg_control=NULL;
    msg.msg_controllen=0;

#endif

    return (* sock->sops.connection.sendmsg)(sock, &msg);
}

static unsigned int get_size_info(struct osns_socket_s *tosend, struct osns_control_info_s *info)
{
    info->code=OSNS_CONTROL_TYPE_OSNS_SOCKET;
    info->info.osns_socket.type=tosend->type;
    info->info.osns_socket.flags=tosend->flags;

    return write_osns_control_info(NULL, 0, info);
}

int osns_reply_mounted(struct osns_receive_s *r, uint32_t id, struct osns_socket_s *tosend)
{
    char data[14];
    unsigned int pos=4;
    struct osns_control_info_s info;
    unsigned int len=4 + 1 + 4 + get_size_info(tosend, &info);

    data[pos]=OSNS_MSG_MOUNTED;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    /* CTRL INFO */

    pos+=write_osns_control_info(&data[pos], (len-pos), &info);
    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, send_data_cb_socket, (void *) tosend);
    // return (* r->send)(r, data, pos, NULL, NULL);

}

int osns_reply_umounted(struct osns_receive_s *r, uint32_t id)
{
    char data[9];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_UMOUNTED;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, NULL, NULL);

}

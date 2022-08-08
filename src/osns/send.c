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

#include "osns-protocol.h"
#include "receive.h"
#include "send.h"
#include "control.h"

int send_osns_msg_init(struct osns_receive_s *r, unsigned int version, unsigned int flags)
{
    unsigned int size=4 + 1 + 8;
    char data[size];
    unsigned int pos=4;

    logoutput_debug("send_osns_msg_init: version %u services %u", version, flags);

    data[pos]=OSNS_MSG_INIT;
    pos++;

    store_uint32(&data[pos], version);
    pos+=4;

    store_uint32(&data[pos], flags);
    pos+=4;

    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, NULL, NULL);
}


/* send an OPENQUERY message
    this maybe about something like services, mountinfo or anything else */

int send_osns_msg_openquery(struct osns_receive_s *r, uint32_t id, struct name_string_s *command, unsigned int flags, unsigned int valid, struct osns_record_s *attr)
{
    unsigned int size=4 + 1 + 4 + 4 + command->len + 4 + 4 + ((attr) ? attr->len : 0);
    char data[size];
    unsigned int pos=4;

    logoutput_debug("send_osns_msg_openquery: id %u", id);

    data[pos]=OSNS_MSG_OPENQUERY;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    pos+=write_name_string(&data[pos], size-pos, 'n', (void *) command);

    store_uint32(&data[pos], flags);
    pos+=4;

    store_uint32(&data[pos], valid);
    pos+=4;

    if (attr && attr->len>0) {

	store_uint16(&data[pos], attr->len);
	pos+=2;
	memcpy(&data[pos], attr->data, attr->len);
	pos+=attr->len;

    } else {

	store_uint16(&data[pos], 0);
	pos+=2;

    }

    store_uint32(&data[0], pos-4);
    return (* r->send)(r, data, pos, NULL, NULL);

}

int send_osns_msg_readquery(struct osns_receive_s *r, uint32_t id, struct name_string_s *handle, unsigned int size, unsigned int offset)
{
    unsigned int tmp=4 + 1 + 4 + 1 + handle->len + 4 + 4;
    char data[tmp];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_READQUERY;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    pos+=write_name_string(&data[pos], tmp-pos, 'n', (void *) handle);

    store_uint32(&data[pos], size);
    pos+=4;
    store_uint32(&data[pos], offset);
    pos+=4;

    store_uint32(&data[0], pos-4);
    return (* r->send)(r, data, pos, NULL, NULL);
}

int send_osns_msg_closequery(struct osns_receive_s *r, uint32_t id, struct name_string_s *handle)
{
    unsigned int size=4 + 1 + 4 + 1 + handle->len;
    char data[size];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_CLOSEQUERY;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;
    pos+=write_name_string(&data[pos], size-pos, 'n', (void *) handle);

    store_uint32(&data[0], pos-4);
    return (* r->send)(r, data, pos, NULL, NULL);
}

int send_osns_msg_mountcmd(struct osns_receive_s *r, uint32_t id, unsigned char type, unsigned int maxread)
{
    unsigned int size=4 + 1 + 4 + 1 + 4;
    char data[size];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_MOUNTCMD;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    data[pos]=type;
    pos++;

    store_uint32(&data[pos], maxread);
    pos+=4;

    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, NULL, NULL);
}

int send_osns_msg_umountcmd(struct osns_receive_s *r, uint32_t id, unsigned char type)
{
    unsigned int size=4 + 1 + 4 + 1;
    char data[size];
    unsigned int pos=4;

    data[pos]=OSNS_MSG_MOUNTCMD;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    data[pos]=type;
    pos++;

    store_uint32(&data[0], pos-4);
    return (* r->send)(r, data, pos, NULL, NULL);
}

/* send a SETWATCH message
    this maybe about something like services, mountinfo or anything else */

int send_osns_msg_setwatch(struct osns_receive_s *r, uint32_t id, struct name_string_s *command)
{
    unsigned int size=4 + 1 + 4 + 4 + command->len;
    char data[size];
    unsigned int pos=4;

    logoutput_debug("send_osns_msg_setwatch: id %u", id);

    data[pos]=OSNS_MSG_SETWATCH;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    pos+=write_name_string(&data[pos], size-pos, 'n', (void *) command);
    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, NULL, NULL);

}

int send_osns_msg_rmwatch(struct osns_receive_s *r, uint32_t id, uint32_t watchid)
{
    unsigned int size=4 + 1 + 4 + 4;;
    char data[size];
    unsigned int pos=4;

    logoutput_debug("send_osns_msg_rmwatch: id %u", id);

    data[pos]=OSNS_MSG_RMWATCH;
    pos++;

    store_uint32(&data[pos], id);
    pos+=4;

    store_uint32(&data[pos], watchid);
    pos+=4;
    store_uint32(&data[0], pos-4);

    return (* r->send)(r, data, pos, NULL, NULL);

}
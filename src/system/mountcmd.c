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
#include "libosns-connection.h"

#include "osns-protocol.h"

#include "receive.h"
#include "osns/reply.h"
#include "mount.h"
#include "fuse.h"

/*
    receive a request to mount a fuse fs

    byte				type
    uint32				maxread

    5 bytes

*/

void process_msg_mountcmd(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    struct osns_mount_s *om=NULL;
    struct system_timespec_s expire;
    unsigned int status=OSNS_STATUS_PROTOCOLERROR;
    unsigned int pos=0;
    unsigned char type=0;
    unsigned int maxread=0;
    struct bevent_s *bevent=NULL;

    logoutput_debug("process_msg_mountcmd: (len=%u)", len);
    if (len<5) goto errorout;

    type=(unsigned char) data[pos];
    pos++;

    if (type==0 || (type != OSNS_MOUNT_TYPE_NETWORK && type != OSNS_MOUNT_TYPE_DEVICES)) {

	status=OSNS_STATUS_INVALIDFLAGS;
	logoutput_debug("process_msg_mountcmd: received invalid type %u", type);
	goto errorout;

    }

    maxread=get_uint32(&data[pos]);
    pos+=4;
    logoutput_debug("process_msg_mountcmd: (type=%u maxread=%u)", type, maxread);

    /* some protection here while sending the osns mount */

    status=OSNS_STATUS_NOTFOUND;
    om=mount_fuse_filesystem(sc, r->signal, type, maxread, &status);
    if (om==NULL) goto errorout;

    /* mounted -> do the init phase here:
	- add socket to eventloop
	- read if data is available
	- reply with init
	- remove socket from eventloop */

    bevent=create_fd_bevent(NULL, (void *) &om->receive);
    if (bevent==NULL) {

	status=OSNS_STATUS_SYSTEMERROR;
	goto errorout;

    }

    init_system_fuse();

    set_bevent_cb(bevent, BEVENT_FLAG_CB_DATAAVAIL, handle_fuse_data_event);
    set_bevent_cb(bevent, BEVENT_FLAG_CB_CLOSE, handle_fuse_close_event);
    set_bevent_cb(bevent, BEVENT_FLAG_CB_ERROR, handle_fuse_close_event);
    set_bevent_osns_socket(bevent, &om->sock);
    add_bevent_watch(bevent);

    /* wait for the init phase to complete ... INIT or ERROR ... */

    get_current_time_system_time(&expire);
    system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 2);

    if (signal_wait_flag_set(om->receive.loop->signal, &om->receive.flags, FUSE_RECEIVE_FLAG_INIT, &expire)==0) {

	logoutput_debug("process_msg_mountcmd: init phase completed");

    } else {

	logoutput_debug("process_msg_mountcmd: init phase not completed (%s)", ((om->receive.flags & FUSE_RECEIVE_FLAG_ERROR) ? "error" : "unknown reason"));
	status=OSNS_STATUS_SYSTEMERROR;
	goto errorout;

    }

    if (osns_reply_mounted(r, id, &om->sock)>0) {

	logoutput_debug("process_msg_mountcmd: send mounted");

    } else {

	logoutput_debug("process_msg_mountcmd: failed to send mounted");

    }

    /* when send ... it's enough to remove the watch from the eventloop ... */

    remove_bevent_watch(bevent, 0);

    return;

    errorout:
    if (bevent) remove_bevent_watch(bevent, 0); /* always remove from eventloop -> otherwise always events reported */
    osns_reply_status(r, id, status, NULL, 0);

}

void process_msg_umountcmd(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    unsigned int status=OSNS_STATUS_PROTOCOLERROR;
    unsigned int pos=0;
    unsigned char type=0;

    logoutput_debug("process_msg_umountcmd: (len=%u)", len);
    if (len<1) goto errorout;

    type=(unsigned char) data[pos];
    pos++;

    if (type==0 || (type != OSNS_MOUNT_TYPE_NETWORK && type != OSNS_MOUNT_TYPE_DEVICES)) {

	status=OSNS_STATUS_INVALIDFLAGS;
	logoutput_debug("process_msg_umountcmd: received invalid type", type);
	goto errorout;

    }

    status=OSNS_STATUS_NOTFOUND;

    if (umount_fuse_filesystem(sc, r->signal, type)) {

	if (osns_reply_umounted(r, id)>0) {

	    logoutput_debug("process_msg_umountcmd: send umounted");

	} else {

	    logoutput_debug("process_msg_umountcmd: failed to send umounted");

	}

	return;

    }

    errorout:
    osns_reply_status(r, id, status, NULL, 0);

}


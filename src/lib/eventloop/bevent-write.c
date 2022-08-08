/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include <time.h>
#include <sys/wait.h>

#include "libosns-misc.h"
#include "libosns-log.h"
#include "libosns-socket.h"

#include "beventloop.h"
#include "bevent.h"
#include "bevent-write.h"

/* wait for a write buffer to become available/writable
    this wait will return:
    -1:			error
    0:			timedout, still blocked
    1:			writable
*/

static int wait_connection_write_unblock(struct bevent_s *bevent, struct shared_signal_s *signal, struct system_timespec_s *overall_expire)
{
    struct system_socket_s *sock=bevent->sock;
    struct system_timespec_s expire;
    int result=0;

    signal_lock(signal);

    checkwaitLabel:

    if ((bevent->flags & BEVENT_FLAG_BLOCKED)==0) {

	signal_unlock(signal);
	return 1;

    } else if ((bevent->flags & BEVENT_FLAG_ENABLED)==0 || (sock->flags & SOCKET_STATUS_OPEN)==0) {

	signal_unlock(signal);
	return -1;

    }

    get_current_time_system_time(&expire);

    if (system_time_test_earlier(&expire, overall_expire)>0) {

	system_time_add(&expire, SYSTEM_TIME_ADD_CENTI, 1); /* add 1 hundreth (==1/100) second */

	int tmp=signal_condtimedwait(signal, &expire);
	if (tmp==0) goto checkwaitLabel;
	result=((tmp==ETIMEDOUT) ? 0 : -1);

    }

    /* overall expired */
    signal_unlock(signal);

    result=((result==0) ? ((bevent->flags & BEVENT_FLAG_BLOCKED) ? 0  : 1) : result);
    return result;

}

static int test_write_op_expired(struct system_timespec_s *overall_expire)
{
    struct system_timespec_s current;

    get_current_time_system_time(&current);
    return ((system_time_test_earlier(&current, overall_expire)>0) ? 0 : 1);
};

int write_socket_signalled(struct bevent_s *bevent, struct bevent_write_data_s *bdata, int (* write_cb)(struct system_socket_s *sock, char *data, unsigned int size, void *ptr))
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);
    struct shared_signal_s *signal=((loop) ? loop->signal : get_default_shared_signal());
    struct system_timespec_s overall_expire=SYSTEM_TIME_INIT;
    int result=-1;
    char *data=bdata->data;

    bdata->byteswritten=0;
    get_current_time_system_time(&overall_expire);
    system_time_add_time(&overall_expire, &bdata->timeout);

    dowriteLabel:

    bevent->flags &= ~ BEVENT_FLAG_BLOCKED;
    result=(* write_cb)(bevent->sock, &data[bdata->byteswritten], (unsigned int)(bdata->size - bdata->byteswritten), bdata->ptr);

    if (result==-1) {
	unsigned int error=errno;

	if ((error==EAGAIN || error==EWOULDBLOCK)) {
	    int tmp=0;

	    /* wait for a "unblock" signal from eventloop the buffer is writable (again?)
		this wait will also return when expired */

	    bevent->flags |= BEVENT_FLAG_BLOCKED;
	    tmp=wait_connection_write_unblock(bevent, signal, &overall_expire);

	    if (tmp==1) {

		goto dowriteLabel; /* try again */

	    } else if (tmp==0) {

		if (test_write_op_expired(&overall_expire)==0) goto dowriteLabel;

	    }

	} else {

	    /* some error */

	    bdata->flags |= WRITE_BEVENT_FLAG_ERROR;
	    logoutput_error("bevent_write_signalled: unknown error %i (%s)", error, strerror(error));

	}

    } else if (result>0) {

	bdata->byteswritten += result;
	if ((bdata->byteswritten < bdata->size) && (test_write_op_expired(&overall_expire)==0)) goto dowriteLabel;

    } else if (result==0) {

	bdata->flags |= WRITE_BEVENT_FLAG_CLOSE;
	goto disconnectLabel;

    }

    return bdata->byteswritten;

    disconnectLabel:
    return 0;

    errorLabel:
    return -1;

}

static void signal_blocked_bevent(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct beventloop_s *loop=get_eventloop_bevent(bevent);
    struct shared_signal_s *signal=((loop) ? loop->signal : get_default_shared_signal());

    /* signal the bevent it is unblocked */

    signal_lock(signal);

    get_current_time_system_time(&bevent->unblocked);
    bevent->flags &= ~BEVENT_FLAG_BLOCKED;

    signal_broadcast(signal);
    signal_unlock(signal);

}

void enable_bevent_write_watch(struct bevent_s *bevent)
{

    if (bevent) {

	bevent->cb_writeable=signal_blocked_bevent;
	bevent->flags |= BEVENT_FLAG_CB_WRITEABLE;

    }

}

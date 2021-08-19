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

#include "main.h"
#include "log.h"
#include "misc.h"
#include "threads.h"

#include "workspace.h"
#include "sftp/common-protocol.h"
#include "common.h"
#include "time.h"
#include "usermapping.h"
#include "extensions.h"
#include "init.h"
#include "recv.h"

/*	callback when the backend (=sftp_client) is "unmounted" by fuse
	this callback is used for the "main" interface pointing to the home
	directory on the server */

static int _signal_ctx2sftp(struct sftp_client_s **p_sftp, const char *what, struct ctx_option_s *option)
{
    struct sftp_client_s *sftp=*p_sftp;
    struct common_signal_s *signal=sftp->signal.signal;

    logoutput("signal_ctx2sftp: what %s", what);

    if (strncmp(what, "command:", 8)==0) {
	unsigned pos=8;

	/* forward these also to the connection */

	if (strncmp(&what[pos], "close:", 6)==0 || strncmp(&what[pos], "disconnect:", 11)==0 ||
	    strncmp(&what[pos], "clear:", 6)==0 || strncmp(&what[pos], "free:", 5)==0) {

	    if ((* sftp->context.signal_sftp2conn)(sftp, what, option)>=0) {

		logoutput("signal_sftp_interface: forwarded %s to connection", what);

	    } else {

		logoutput("signal_sftp_interface: failed to forward %s", what);

	    }

	}

	if (strncmp(&what[pos], "close:", 6)==0 || strncmp(&what[pos], "disconnect:", 11)==0) {

	    signal_lock(signal);

	    if (sftp->signal.flags & SFTP_SIGNAL_FLAG_DISCONNECT) {

		signal_unlock(signal);
		return 0;

	    }

	    /* no other actions required than this for stfp, closing sftp
		is just about closing the connection/channel */

	    sftp->signal.flags |= SFTP_SIGNAL_FLAG_DISCONNECTED;
	    signal_broadcast(signal);
	    signal_unlock(signal);

	}

	if (strncmp(&what[pos], "clear:", 6)==0 || strncmp(&what[pos], "free:", 5)==0) {

	    signal_lock(signal);

	    if (sftp->signal.flags & SFTP_SIGNAL_FLAG_CLEAR) {

		signal_unlock(signal);
		return 0;

	    }

	    sftp->signal.flags |= SFTP_SIGNAL_FLAG_CLEARING;
	    signal_unlock(signal);

	    clear_sftp_client(sftp);

	    signal_lock(signal);
	    sftp->signal.flags |= SFTP_SIGNAL_FLAG_CLEARED;
	    signal_unlock(signal);

	    free_sftp_client(p_sftp);

	}

    }

    /* what more ? */

    return 0;

}

static int _signal_sftp2ctx_default(struct sftp_client_s *sftp, const char *what, struct ctx_option_s *option)
{
    /* does nothing since here is not known where to send it to */
    return 0;
}

/* signal the sftp subsystem about events like disconnect */

static int _signal_conn2sftp(struct sftp_client_s *sftp, const char *what, struct ctx_option_s *option)
{
    struct common_signal_s *signal=sftp->signal.signal;

    logoutput("signal_conn2sftp: what %s", what);

    if (strncmp(what, "event:", 6)==0) {
	unsigned pos=6;

	if (strncmp(&what[pos], "close:", 6)==0 || strncmp(&what[pos], "disconnect:", 11)==0) {

	    signal_lock(signal);

	    if (sftp->signal.flags & SFTP_SIGNAL_FLAG_DISCONNECT) {

		signal_unlock(signal);
		return 0;

	    }

	    /* forward these also to the connection */

	    if ((* sftp->context.signal_sftp2ctx)(sftp, what, option)>=0) {

		logoutput("signal_conn2sftp: forwarded %s to context", what);

	    } else {

		logoutput("signal_conn2sftp: failed to forward %s", what);

	    }

	    /* no other actions required than this for stfp, closing sftp
		is just about closing the connection/channel */

	    sftp->signal.flags |= SFTP_SIGNAL_FLAG_DISCONNECTED;
	    signal_broadcast(signal);
	    signal_unlock(signal);

	}

    }

    /* what more ? */

    return 0;
}

static int _signal_sftp2conn_default(struct sftp_client_s *sftp, const char *what, struct ctx_option_s *option)
{
    /* does nothing since here is not known where to send it to */
    return 0;
}

static int _send_data_default(struct sftp_client_s *s, char *buffer, unsigned int size, unsigned int *seq, struct list_element_s *list)
{
    /* does nothing since here is not known where to send it to */
    return -1;
}

void init_sftp_default_context(struct sftp_client_s *sftp)
{
    struct sftp_context_s *context=&sftp->context;

    context->signal_ctx2sftp=_signal_ctx2sftp;
    context->signal_sftp2ctx=_signal_sftp2ctx_default;
    context->signal_conn2sftp=_signal_conn2sftp;
    context->signal_sftp2conn=_signal_sftp2conn_default;
    context->send_data=_send_data_default;
    context->receive_data=receive_sftp_data;
}

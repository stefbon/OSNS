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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

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

static int _signal_ctx2sftp(struct sftp_client_s **p_sftp, const char *what, struct io_option_s *option, unsigned int type)
{
    struct sftp_client_s *sftp=*p_sftp;
    struct shared_signal_s *signal=sftp->signal.signal;

    logoutput("signal_ctx2sftp: what %s", what);

    if (strncmp(what, "command:", 8)==0) {
	unsigned pos=8;

	/* forward these also to the connection */

	if (strncmp(&what[pos], "close:", 6)==0 || strncmp(&what[pos], "disconnect:", 11)==0 ||
	    strncmp(&what[pos], "clear:", 6)==0 || strncmp(&what[pos], "free:", 5)==0) {

	    if ((* sftp->context.signal_sftp2ctx)(sftp, what, option, type)>=0) {

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

static int _signal_sftp2ctx_default(struct sftp_client_s *sftp, const char *what, struct io_option_s *option, unsigned int type)
{
    /* does nothing since here is not known where to send it to */
    return 0;
}

static int _send_data_default(struct sftp_client_s *s, char *buffer, unsigned int size, unsigned int *seq, struct list_element_s *list)
{
    /* does nothing since here is not known where to send it to */
    return -1;
}

/* conversion functions for paths , from the protocol to local and vice versa
    20220803: for now no conversion
    TODO: UTF-8 to local, local to UTF-8
    and initialization and make use of the extensions
    "filename-charset" and "filename-translation-control"

    See:

    https://datatracker.ietf.org/doc/html/draft-ietf-secsh-filexfer-13#section-6
    "SSH File Transfer Protocol draft-ietf-secsh-filexfer-13"
*/

static unsigned int _get_required_path_size_p2l(struct sftp_client_s *s, unsigned int len)
{
    logoutput_debug("_get_required_path_size_p2l: len %u", len);
    return len;
}

static unsigned int _get_required_path_size_l2p(struct sftp_client_s *s, unsigned int len)
{
    return len;
}

static int _convert_path_p2l(struct sftp_client_s *s, char *buffer, unsigned int size, char *data, unsigned int len)
{

    logoutput_debug("_convert_path_p2l: size %u len %u", size, len);

    memcpy(buffer, data, len);
    return (int) len;
}

static int _convert_path_l2p(struct sftp_client_s *s, char *buffer, unsigned int size, char *data, unsigned int len)
{
    memcpy(buffer, data, len);
    return (int) len;
}

static unsigned char break_request_default(struct sftp_client_s *s, unsigned int *p_status)
{
    return 0;
}

void init_sftp_default_context(struct sftp_client_s *sftp)
{
    struct sftp_context_s *context=&sftp->context;

    context->signal_ctx2sftp=_signal_ctx2sftp;
    context->signal_sftp2ctx=_signal_sftp2ctx_default;
    context->break_request=break_request_default;
    context->send_data=_send_data_default;
    context->recv_data=receive_sftp_data;
    context->get_required_path_size_p2l=_get_required_path_size_p2l;
    context->get_required_path_size_l2p=_get_required_path_size_l2p;
    context->convert_path_p2l=_convert_path_p2l;
    context->convert_path_l2p=_convert_path_l2p;

}

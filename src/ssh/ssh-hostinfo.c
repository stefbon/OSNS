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

#include "libosns-basic-system-headers.h"

#include "pwd.h"
#include "grp.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-interface.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "users/mapping.h"

static void correct_time_none(struct ssh_session_s *session, struct system_timespec_s *time)
{
    /* does nothing */
}

/* correct the time when difference is positive (other side is behind) */

static void correct_time_positive(struct ssh_session_s *session, struct system_timespec_s *time)
{
    system_time_add_time(time, &session->hostinfo.delta);
}

/* correct the time when difference is negative (other side is ahead) */

static void correct_time_negative(struct ssh_session_s *session, struct system_timespec_s *time)
{
    system_time_substract_time(time, &session->hostinfo.delta);
}

/* set functions used when time on server is behind compared to the client*/

void set_time_correction_server_behind(struct ssh_session_s *session, struct system_timespec_s *delta)
{
    struct ssh_hostinfo_s *hostinfo=&session->hostinfo;

    hostinfo->correct_time_s2c=correct_time_positive;
    hostinfo->correct_time_c2s=correct_time_negative;
    copy_system_time(&hostinfo->delta, delta);
}

/* set functions used when time on server is ahead compared the client*/

void set_time_correction_server_ahead(struct ssh_session_s *session, struct system_timespec_s *delta)
{
    struct ssh_hostinfo_s *hostinfo=&session->hostinfo;

    hostinfo->correct_time_s2c=correct_time_negative;
    hostinfo->correct_time_c2s=correct_time_positive;
    copy_system_time(&hostinfo->delta, delta);
}

/* initialize the mapping of the local users to remote users */

void init_ssh_hostinfo(struct ssh_session_s *session)
{
    struct ssh_hostinfo_s *hostinfo=&session->hostinfo;

    /* correction function for differences in time */

    hostinfo->flags=0;
    hostinfo->delta.st_sec=0;
    hostinfo->delta.st_nsec=0;

    hostinfo->correct_time_s2c=correct_time_none;
    hostinfo->correct_time_c2s=correct_time_none;

    init_ssh_usermapping(session, &session->identity.pwd);

}

void free_ssh_hostinfo(struct ssh_session_s *ssh_session)
{
    struct ssh_hostinfo_s *hostinfo=&ssh_session->hostinfo;
    memset(hostinfo, 0, sizeof(struct ssh_hostinfo_s));
}

/*
    after receiving time from server calculate the time difference to apply to time related messages */

static void set_time_delta(struct ssh_session_s *session, struct system_timespec_s *send, struct system_timespec_s *recv, struct system_timespec_s *server)
{
    struct system_timespec_s delta=SYSTEM_TIME_INIT;
    double send_d=convert_system_time_to_double(send);
    double recv_d=convert_system_time_to_double(recv);
    double server_d=convert_system_time_to_double(server);
    double average_d=0;

    /* send_d and recv_d undicate the time (in double format) when sending and receiving the time correction message
	server_d is the time (double_d) the server has set when responding to this message
	the idea is that the average of the first two is about the same as server_d */

    average_d=((recv_d + send_d) / 2 );

    logoutput("set_time_delta: out %.3f send %.3f recv %.3f delta %.3f", server_d, send_d, recv_d, average_d);

    if (average_d > server_d) {

	/* server is behind */

	convert_system_time_from_double(&delta, (average_d - server_d));
	set_time_correction_server_behind(session, &delta);

    } else {

	/* server is ahead */

	convert_system_time_from_double(&delta, (server_d - average_d));
	set_time_correction_server_ahead(session, &delta);

    }

}

/*
    get basic info from server like:
    - time diff between server and this client
    */

void start_timecorrection_ssh_server(struct ssh_session_s *session)
{
    struct ssh_connection_s *connection=session->connections.main;
    struct system_timespec_s send_c=SYSTEM_TIME_INIT;
    struct io_option_s option;

    signal_lock(connection->setup.signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_HOSTINFO) {

	signal_unlock(connection->setup.signal);
	return;

    }

    connection->setup.flags|=SSH_SETUP_FLAG_HOSTINFO;
    signal_unlock(connection->setup.signal);

    init_io_option(&option, _IO_OPTION_TYPE_BUFFER);
    get_current_time_system_time(&send_c);

    if ((* session->context.signal_ssh2remote)(session, "info:remotetime:", &option, INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION)>=0) {

	if (option.flags & _IO_OPTION_FLAG_ERROR) {

	    if (option.type==_IO_OPTION_TYPE_BUFFER) {
		unsigned int len=option.value.buffer.len;
		char tmp[len + 1];

		memset(tmp, 0, len+1);
		memcpy(tmp, option.value.buffer.ptr, len);
		logoutput("start_timecorrection_ssh_server: error %s", tmp);

	    }

	} else if (option.type==_IO_OPTION_TYPE_BUFFER) {
	    struct system_timespec_s recv_c=SYSTEM_TIME_INIT;
	    unsigned int len=option.value.buffer.len;
	    char tmp[len + 1];
	    char *sep=NULL;

	    /* buffer is in the form:
		1524159292.579901450 */

	    get_current_time_system_time(&recv_c);

	    memset(tmp, 0, len+1);
	    memcpy(tmp, option.value.buffer.ptr, len);
	    tmp[len]='\0';

	    sep=memchr(tmp, '.', len);

	    if (sep) {
		struct system_timespec_s server=SYSTEM_TIME_INIT;

		*sep='\0';

		set_system_time(&server, (system_time_sec_t) atol(tmp), (system_time_nsec_t) atol(sep+1));
		set_time_delta(session, &send_c, &recv_c, &server);

	    } else {

		logoutput("start_timecorrection_ssh_server: no seperator found in %s", tmp);

	    }

	} else {

	    logoutput("start_timecorrection_ssh_server: received not the right format (buffer)");

	}

	(* option.free)(&option);

    }

}

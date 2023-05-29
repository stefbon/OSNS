/*
  2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-receive.h"

/*
    get the remote ssh version
    this string has the form:

    SSH-x.yz-softwareversion<SP>comment<CR><LF>

    - it's also possible that there is no comment, then there is also no <SP>
*/

int read_ssh_protocol_version(struct ssh_session_s *session)
{
    struct ssh_string_s *greeter=&session->data.greeter_server;
    char *sep=NULL;
    char *start=NULL;
    unsigned int left=0;
    int result=-1;

    if (greeter->ptr==NULL || greeter->len==0) return -1;
    logoutput_debug("read_ssh_protocol_version: analyze %.*s", greeter->len, greeter->ptr);

    session->data.remote_version_major=0;
    session->data.remote_version_minor=0;

    start=(char *) (greeter->ptr + strlen(SSH_GREETER_START));
    left=greeter->len - strlen(SSH_GREETER_START);
    sep=memchr(start, '-', left);

    if (sep) {
	unsigned int len=(unsigned int) (sep - start);
	char tmp[len+1];
	char *sep=NULL;

	memcpy(tmp, start, len);
	tmp[len]='\0';

	sep=memchr(tmp, '.', len);

	if (sep) {

	    *sep='\0';
	    session->data.remote_version_minor=atoi(sep+1);

	}

        session->data.remote_version_major=atoi(tmp);
        result=(int) session->data.remote_version_major;

    } else {

	/* error in the version: there should be a '-' */

	logoutput_debug("read_ssh_protocl_version: format error (no - seperator found)");

    }

    logoutput_debug("read_ssh_protocol_version: result %u", result);
    return result;

}

unsigned int get_ssh_protocol_version(struct ssh_session_s *session)
{
    return session->data.remote_version_major;
}

unsigned int get_ssh_protocol_minor(struct ssh_session_s *session)
{
    return session->data.remote_version_minor;
}

static int save_ssh_protocol_greeter_string(struct ssh_session_s *session, char *line, unsigned int len)
{
    struct ssh_string_s *greeter=&session->data.greeter_server;
    return (create_ssh_string(&greeter, len, line, SSH_STRING_FLAG_ALLOC) ? 0 : -1);
}

/*
    read the first data from server
    this is the greeter
    take in account the second ssh message can be attached
*/

void cb_read_socket_ssh_greeter(struct osns_socket_s *sock, uint64_t ctr, void *ptr)
{
    struct ssh_connection_s *sshc=(struct ssh_connection_s *) ptr;
    struct ssh_session_s *session=get_ssh_connection_session(sshc);
    struct ssh_receive_s *r=&sshc->receive;
    unsigned int errcode=0;
    struct system_timespec_s expire;
    char term_crlf[2];

    term_crlf[0]=13;
    term_crlf[1]=10;
    get_ssh_connection_expire_init(sshc, &expire);

    if (signal_set_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD)==0) return;

    logoutput_debug("cb_read_ssh_socket_greeter: %u bytes in buffer", sock->rd.pos);

    while ((get_ssh_protocol_version(session)==0) && (errcode==0)) {
        char line[256]; /* max length per line send (rfc4253#section-4.2)*/
        char *sep=NULL;
        unsigned int len=0;

        memset(line, 0, 256);

        signal_lock(sock->signal);

        while (sep==NULL) {

            sep=memmem(sock->rd.buffer, sock->rd.pos, term_crlf, 2); /* look for the line terminator */

            if (sep) {
                unsigned int size = (unsigned int)(sep - r->buffer); /* size exclusive the terminator (crlf) */

                /* for SSH 2 the line length may not exceed 255
                (note 253 = 255 minus the length of the terminator) */

                if (size > 253) {

                    errcode=EPROTO;
                    goto disconnect;

                }

                memmove(line, sock->rd.buffer, size);
                line[size]='\0';

                logoutput_debug("cb_read_ssh_socket_greeter: read %u size %u", sock->rd.pos, size);

                if ((size + 2) < sock->rd.pos) {
                    unsigned int tmp=size+2;

                    memmove(sock->rd.buffer, (char *)(sock->rd.buffer + tmp), (sock->rd.pos - tmp));
                    sock->rd.pos -= tmp;

                } else {

                    sock->rd.pos = 0;

                }

                break;

            }

            int result=signal_condtimedwait(sock->signal, &expire);

            if (result==ETIMEDOUT) {

                errcode=result;
                signal_unlock(sock->signal);
                goto disconnect;

            }

        }

        signal_unlock(sock->signal);

        len=strlen(line);

        /* see RFC 4253 4.2 Protocol Version Exchange
        server may provide more than one line, only the last starts (or the only) with the SSH- protocol string */

        if ((len > strlen(SSH_GREETER_START)) && memcmp(line, SSH_GREETER_START, strlen(SSH_GREETER_START))==0) {

            logoutput_debug("read_ssh_socket_greeter: found remote protocol %.*s", len, line);

            if (save_ssh_protocol_greeter_string(session, line, len)==-1) {

                errcode=ENOMEM;
                logoutput_debug("read_ssh_buffer_socket: unable to save greeter string");
                goto disconnect;

            }

            if (read_ssh_protocol_version(session)==-1) {

                logoutput_debug("read_ssh_socket_greeter: unable to read protocol version");
                errcode=EPROTO;
                goto disconnect;

            }

            errcode=0;
            break;

        } else if (len>0) {

            logoutput_debug("read_ssh_buffer_socket: found line %.*s", len, line);

        }

    }

    signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);

    if ((errcode==0) && (get_ssh_protocol_version(session)==0)) errcode=EPROTO;

    if (errcode==0) {
        unsigned int tmp=get_ssh_protocol_version(session);

        logoutput_debug("read_ssh_buffer_greeter: found version %u", tmp);

        change_ssh_connection_setup(sshc, "transport", SSH_TRANSPORT_TYPE_GREETER, SSH_GREETER_FLAG_S2C, 0, NULL, NULL);
        set_ssh_receive_behaviour(sshc, "session");
        set_ssh_socket_behaviour(sock, "session");

        (* sock->ctx.read)(sock, ctr, ptr);

    }

    return;

    disconnect:
    (* sock->close)(sock);

}

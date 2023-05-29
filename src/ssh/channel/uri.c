/*
  2018, 2019 Stef Bon <stefbon@gmail.com>

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
#include "ssh-channel.h"
#include "uri.h"

/* functions to translate to check, set and match a channel and an uri */

int check_ssh_channel_uri(char *uri)
{
    unsigned int pos=0;
    unsigned int len=0;
    int result=-1;

    if ((uri==NULL) || (strlen(uri)==0)) return -1;
    len=strlen(uri);

    if (compare_starting_substring(uri, len, "ssh-channel//direct-streamlocal/", &pos)==0) {
        char *path=(char *)(uri + pos);

        len=strlen(path);

        /* uri is like:
            ssh-channel://direct-streamlocal/run/some.sock */

        if ((len>0) && (len < SSH_CHANNEL_STREAMLOCAL_PATH_MAX)) result=0;

    } else if (compare_starting_substring(uri, len, "ssh-channel://direct-tcpip/", &pos)==0) {
	char *target=(char *)(uri + pos);
	char *sep=memchr(target, ':', len - pos);

        /* uri is for example
            ssh-channel://direct-tcpip/somehost:631
            ssh-channel://direct-tcpip/192.168.0.88:9000
        */

        if (sep) result=0;

    } else if ((compare_starting_substring(uri, len, "ssh-channel://session", &pos)==0) && (len==pos || uri[pos]=='/')) {

        /* uri is for example
            ssh-channel://session
            ssh-channel://session/shell
            ssh-channel://session/exec/acommand
            ssh-channel://session/subsystem/name
        */

        if (len <= pos + 1) {

            result=0;

	} else {

	    if (compare_starting_substring(&uri[pos], len-pos, "/shell/", &pos)==0) {

                result=0;

            } else if ((compare_starting_substring(&uri[pos], len-pos, "/exec/", &pos)==0) || (compare_starting_substring(&uri[pos], len-pos, "/subsystem/", &pos)==0)) {
		char *command=(char *)(uri + pos);
		unsigned int tmplen=strlen(command);

		if (tmplen>0 && tmplen<SSH_CHANNEL_SESSION_BUFFER_MAXLEN) result=0;

            }

        }

    }

    return result;

}

int translate_ssh_channel_uri(struct ssh_channel_s *channel, char *uri)
{
    int result=-1;
    unsigned int len=0;
    unsigned int pos=0;

    if (uri==NULL || strlen(uri)==0) return -1;
    len=strlen(uri);

    if (compare_starting_substring(uri, len, "ssh-channel//direct-streamlocal/", &pos)==0) {
	char *path=(char *)(uri + pos);

	if (strlen(path) < SSH_CHANNEL_STREAMLOCAL_PATH_MAX) {

	    strcpy(channel->target.socket.stype.local.path, path);
	    channel->type = SSH_CHANNEL_TYPE_DIRECT;
	    channel->target.socket.type=SSH_CHANNEL_SOCKET_TYPE_STREAMLOCAL;

	}

    } else if (compare_starting_substring(uri, len, "ssh-channel://direct-tcpip/", &pos)==0) {
	char *target=(char *)(uri + pos);
	char *sep=memchr(target, ':', len - pos);

	if (sep) {
	    unsigned int portnr=atoi(sep+1);
	    unsigned int tmplen=(unsigned int)(sep - target);
	    char tmp[tmplen + 1];

	    memset(tmp, 0, tmplen + 1);
	    memcpy(tmp, target, tmplen);

	    if (check_family_ip_address(tmp, "ipv4")) {

		set_host_address(&channel->target.socket.stype.tcpip.address, NULL, tmp, NULL);

	    } else if (check_family_ip_address(tmp, "ipv6")) {

		set_host_address(&channel->target.socket.stype.tcpip.address, NULL, NULL, tmp);

	    } else {

                /* must be a hostname */
		set_host_address(&channel->target.socket.stype.tcpip.address, tmp, NULL, NULL);

	    }

	    channel->type = SSH_CHANNEL_TYPE_DIRECT;
	    channel->target.socket.type=SSH_CHANNEL_SOCKET_TYPE_TCPIP;
	    channel->target.socket.stype.tcpip.portnr=atoi(sep+1);
	    result=len;

	}

    } else if ((compare_starting_substring(uri, len, "ssh-channel://session", &pos)==0) && (len==pos || uri[pos]=='/')) {

        if (len==pos) {

            channel->type = SSH_CHANNEL_TYPE_SESSION;
            channel->target.session.type=SSH_CHANNEL_SESSION_TYPE_NOTSET;
            result=len;

	} else if (len>pos) {

	    if (compare_starting_substring(&uri[pos], len-pos, "/shell/", &pos)==0) {

		channel->type = SSH_CHANNEL_TYPE_SESSION;
		channel->target.session.type=SSH_CHANNEL_SESSION_TYPE_SHELL;
		result=len;

	    } else if (compare_starting_substring(&uri[pos], len-pos, "/exec/", &pos)==0) {
		char *command=(char *)(uri + pos);
		unsigned int tmplen=strlen(command);

		if ((tmplen>0) && (tmplen<SSH_CHANNEL_SESSION_BUFFER_MAXLEN)) {

		    memset(channel->target.session.buffer, 0, SSH_CHANNEL_SESSION_BUFFER_MAXLEN);
		    memcpy(channel->target.session.buffer, command, tmplen);
		    channel->type = SSH_CHANNEL_TYPE_SESSION;
		    channel->target.session.type=SSH_CHANNEL_SESSION_TYPE_EXEC;
		    result=len;

		}

	    } else if (compare_starting_substring(&uri[pos], len-pos, "/subsystem/", &pos)==0) {
		char *name=(char *)(uri + pos);
		unsigned int tmplen=strlen(name);

		if (tmplen>0 && tmplen<SSH_CHANNEL_SESSION_BUFFER_MAXLEN) {

		    memset(channel->target.session.buffer, 0, SSH_CHANNEL_SESSION_BUFFER_MAXLEN);
		    memcpy(channel->target.session.buffer, name, tmplen);

		    channel->type = SSH_CHANNEL_TYPE_SESSION;
		    channel->target.session.type=SSH_CHANNEL_SESSION_TYPE_SUBSYSTEM;
		    result=len;

		}

	    }

	}

    }

    return result;

}

int match_ssh_channel_uri(struct ssh_channel_s *channel, char *uri)
{
    int result=-1;
    unsigned int pos=0;
    unsigned int len=(uri) ? strlen(uri) : 0;

    if (channel->type==SSH_CHANNEL_TYPE_SESSION) {

        logoutput_debug("match_ssh_channel_uri: channel type session (uri %s)", uri);

        if ((compare_starting_substring(uri, len, "ssh-channel://session", &pos)==0) && (pos==len || uri[pos]=='/')) {

            if (channel->target.session.type==SSH_CHANNEL_SESSION_TYPE_NOTSET) {

                logoutput_debug("match_ssh_channel_uri: channel session type notset");
                if (len <= pos + 1) result=0;

            } else if (channel->target.session.type==SSH_CHANNEL_SESSION_TYPE_SHELL) {

                logoutput_debug("match_ssh_channel_uri: channel session type shell");
                if (compare_starting_substring(&uri[pos], (len-pos), "/shell/", &pos)==0) result=0;

            } else if (channel->target.session.type==SSH_CHANNEL_SESSION_TYPE_EXEC) {

                logoutput_debug("match_ssh_channel_uri: channel session type exec");
                if (compare_starting_substring(&uri[pos], (len-pos), "/exec/", &pos)==0) result=0;

            } else if (channel->target.session.type==SSH_CHANNEL_SESSION_TYPE_SUBSYSTEM) {

                logoutput_debug("match_ssh_channel_uri: channel session type subsystem");
                if (compare_starting_substring(&uri[pos], (len-pos), "/subsystem/", &pos)==0) result=0;

            }

        }

    } else if (channel->type==SSH_CHANNEL_TYPE_DIRECT) {

        if (channel->target.socket.type==SSH_CHANNEL_SOCKET_TYPE_STREAMLOCAL) {

            if (compare_starting_substring(uri, len, "ssh-channel//direct-streamlocal/", &pos)==0) result=0;

        } else if (channel->target.socket.type==SSH_CHANNEL_SOCKET_TYPE_TCPIP) {

            if (compare_starting_substring(uri, len, "ssh-channel://direct-tcpip/", &pos)==0) result=0;

        }

    }

    return result;

}

void set_orig_address_ssh_channel_direct_tcpip(struct ssh_channel_s *channel, struct ip_address_s *ip, unsigned int portnr)
{

    if ((channel->type==SSH_CHANNEL_TYPE_DIRECT) && (channel->target.socket.type==SSH_CHANNEL_SOCKET_TYPE_TCPIP)) {

	if (ip) memcpy(&channel->target.socket.stype.tcpip.orig_ip, ip, sizeof(struct ip_address_s));
	if (portnr>0) channel->target.socket.stype.tcpip.orig_portnr=portnr;

    }

}

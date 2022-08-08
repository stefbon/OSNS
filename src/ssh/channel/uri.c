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

/* functions to translate an uri in a specific channel
    20170720: only sockets://path and tcp/ip connections to a host:port are supported
*/

int translate_channel_uri(struct ssh_channel_s *channel, char *uri)
{
    int result=-1;

    if (uri==NULL || strlen(uri)==0) return -1;

    if (compare_starting_substring(uri, strlen(uri), "direct-streamlocal://")==0) {
	unsigned int len=strlen("direct-streamlocal://");
	char *path=(char *)(uri + len - 1);					/* -1: start at the second slash to get the absolute path */

	if (strlen(path) < _CHANNEL_DIRECT_STREAMLOCAL_PATH_MAX) {

	    strcpy(channel->target.direct_streamlocal.path, path);
	    channel->type = _CHANNEL_TYPE_DIRECT_STREAMLOCAL;

	}

    } else if (compare_starting_substring(uri, strlen(uri), "direct-tcpip://")==0) {
	unsigned int len=strlen("direct-tcpip://");
	char *target=(char *)(uri + len);
	char *sep=memchr(target, ':', strlen(uri) - len);

	if (sep) {
	    unsigned int portnr=atoi(sep+1);
	    unsigned int tmplen=(unsigned int)(sep - target);
	    char tmp[tmplen + 1];

	    memset(tmp, 0, tmplen + 1);
	    memcpy(tmp, target, tmplen);

	    if (check_family_ip_address(tmp, "ipv4")) {

		set_host_address(&channel->target.direct_tcpip.address, NULL, tmp, NULL);

	    } else if (check_family_ip_address(tmp, "ipv6")) {

		set_host_address(&channel->target.direct_tcpip.address, NULL, NULL, tmp);

	    } else {

		set_host_address(&channel->target.direct_tcpip.address, tmp, NULL, NULL);

	    }

	    channel->type = _CHANNEL_TYPE_DIRECT_TCPIP;
	    channel->target.direct_tcpip.portnr=atoi(sep+1);
	    result=strlen(uri) - len;

	}

    } else if (compare_starting_substring(uri, strlen(uri), "session-exec://")==0) {
	unsigned int len=strlen("session-exec://");
	char *command=(char *)(uri + len);
	unsigned int tmplen=strlen(command);

	if (tmplen>0 && tmplen<_CHANNEL_SESSION_BUFFER_MAXLEN) {

	    memset(channel->target.session.buffer, 0, _CHANNEL_SESSION_BUFFER_MAXLEN);
	    memcpy(channel->target.session.buffer, command, tmplen);
	    channel->type = _CHANNEL_TYPE_SESSION;
	    channel->target.session.type=_CHANNEL_SESSION_TYPE_EXEC;
	    result=strlen(uri) - len;

	}

    } else if (compare_starting_substring(uri, strlen(uri), "session-shell://")==0) {

	channel->type = _CHANNEL_TYPE_SESSION;
	channel->target.session.type=_CHANNEL_SESSION_TYPE_SHELL;

    } else if (compare_starting_substring(uri, strlen(uri), "session-subsystem://")==0) {
	unsigned int len=strlen("session-subsystem://");
	char *name=(char *)(uri + len);
	unsigned int tmplen=strlen(name);

	if (tmplen>0 && tmplen<_CHANNEL_SESSION_BUFFER_MAXLEN) {

	    memset(channel->target.session.buffer, 0, _CHANNEL_SESSION_BUFFER_MAXLEN);
	    memcpy(channel->target.session.buffer, name, tmplen);

	    channel->type = _CHANNEL_TYPE_SESSION;
	    channel->target.session.type=_CHANNEL_SESSION_TYPE_SUBSYSTEM;
	    result=strlen(uri) - len;

	}

    }

    return result;

}

void set_orig_address_ssh_channel_direct_tcpip(struct ssh_channel_s *channel, struct ip_address_s *ip, unsigned int portnr)
{

    if (channel->type==_CHANNEL_TYPE_DIRECT_TCPIP) {

	if (ip) memcpy(&channel->target.direct_tcpip.orig_ip, ip, sizeof(struct ip_address_s));
	if (portnr>0) channel->target.direct_tcpip.orig_portnr=portnr;

    }

}

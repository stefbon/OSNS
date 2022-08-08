/*
  2017 Stef Bon <stefbon@gmail.com>

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
#include "utils.h"
#include "services.h"

struct tmp_service_s {
    char				*name;
    unsigned int			type;
    unsigned int			flags;
};

static struct tmp_service_s services[] = {
    {	.name 	= "ssh",
	.type	= NETWORK_SERVICE_TYPE_SSH,
	.flags	= NETWORK_SERVICE_FLAG_TRANSPORT | NETWORK_SERVICE_FLAG_CRYPTO | NETWORK_SERVICE_FLAG_CONNECTION},
    {	.name 	= "sftp",
	.type	= NETWORK_SERVICE_TYPE_SFTP,
	.flags	= NETWORK_SERVICE_FLAG_FILESYSTEM},
    {	.name 	= "smb",
	.type	= NETWORK_SERVICE_TYPE_SMB,
	.flags	= NETWORK_SERVICE_FLAG_FILESYSTEM},
    {	.name 	= "nfs",
	.type	= NETWORK_SERVICE_TYPE_NFS,
	.flags	= NETWORK_SERVICE_FLAG_FILESYSTEM},
    {	.name 	= "webdav",
	.type	= NETWORK_SERVICE_TYPE_WEBDAV,
	.flags	= NETWORK_SERVICE_FLAG_FILESYSTEM},
    {	.name	= NULL, .type	= 0, .flags	= 0}};


#ifdef __linux__

#include <netdb.h>

char *get_system_network_service_name(unsigned int port)
{
    struct servent *srv=getservbyport((int) port, NULL);
    return ((srv) ? srv->s_name : NULL);
}

#else

char *get_system_network_service_name(unsigned int port)
{
    return NULL;
}

#endif

char *get_network_service_name(unsigned int type)
{
    char *name=NULL;
    unsigned int i=0;

    while (services[i].name) {

	if (services[i].type==type) {

	    name=services[i].name;
	    break;

	}

	i++;

    }

    return name;

}

unsigned int get_network_service_type(char *name, unsigned int len, unsigned int *p_flags)
{
    unsigned int type=0;

    if (name) {

	if (len==0) len=strlen(name);

    } else {

	return 0;

    }

    unsigned int i=0;

    while (services[i].name) {

	if ((strlen(services[i].name)==len) && (strncmp(services[i].name, name, len)==0)) {

	    type=services[i].type;
	    if (p_flags) *p_flags=services[i].flags;
	    break;

	}

	i++;

    }

    return type;
}

unsigned int guess_network_service_from_port(unsigned int port)
{
    char *name=get_network_service_name(port);
    unsigned int type=0;

    if (name==NULL) {

	logoutput_warning("guess_network_service_from_port: port %u not reckognized", port);

    } else if (strcmp(name, "ssh")==0) {

	type = NETWORK_SERVICE_TYPE_SSH;

    } else if (strcmp(name, "sftp")==0) {

	type = NETWORK_SERVICE_TYPE_SFTP;

    } else if (strcmp(name, "nfs")==0) {

	type = NETWORK_SERVICE_TYPE_NFS;

    } else if (strcmp(name, "netbios-dgm")==0 || strcmp(name, "microsoft-ds")==0) {

	type = NETWORK_SERVICE_TYPE_SMB;

    } else if (strcmp(name, "davsrc")==0) {

	type = NETWORK_SERVICE_TYPE_WEBDAV;

    } else {

	logoutput_warning("guess_network_service_from_port: service %s (port=%u) not supported", name, port);

    }

    return type;
}

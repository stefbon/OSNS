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

*/

#ifndef _LIB_NETWORK_SERVICES_H
#define _LIB_NETWORK_SERVICES_H

#define NETWORK_SERVICE_TYPE_SSH			1
#define NETWORK_SERVICE_TYPE_SFTP			2
#define NETWORK_SERVICE_TYPE_SMB			3
#define NETWORK_SERVICE_TYPE_NFS			4
#define NETWORK_SERVICE_TYPE_WEBDAV			5
#define NETWORK_SERVICE_TYPE_IPP                        6
#define NETWORK_SERVICE_TYPE_HTTP                       7
#define NETWORK_SERVICE_TYPE_HTTPS                      8

#define NETWORK_SERVICE_FLAG_TRANSPORT			1
#define NETWORK_SERVICE_FLAG_CRYPTO			2
#define NETWORK_SERVICE_FLAG_CONNECTION			4
#define NETWORK_SERVICE_FLAG_FILESYSTEM			8

/* prototypes */

char *get_system_network_service_name(unsigned int port);
char *get_network_service_name(unsigned int type);
const char *get_network_connection_type(unsigned int type);

unsigned int get_network_service_type(char *name, unsigned int len, unsigned int *p_flags);
unsigned int guess_network_service_from_port(unsigned int port);

#endif

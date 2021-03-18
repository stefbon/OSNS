/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef OSNS_SOCKET_H
#define OSNS_SOCKET_H

#include "network.h"

#define OSNS_MSG_VERSION				1
#define OSNS_MSG_DISCONNECT				2
#define OSNS_MSG_NOTSUPPORTED				3

#define OSNS_MSG_SERVICE_REQUEST			10
#define OSNS_MSG_SERVICE_ACCEPT				11
#define OSNS_MSG_SERVICE_DENY				12

#define OSNS_MSG_COMMAND				20
#define OSNS_MSG_COMMAND_SUCCESS			21
#define OSNS_MSG_COMMAND_FAILURE			22

#define OSNS_COMMAND_ENUM_SERVICES			"enum-services@osns.net"
#define OSNS_COMMAND_INFO_PATH				"info-path@osns.net"

/* SSH */

#define OSNS_SERVICE_REQUEST_SSH_CONNECTION		"ssh-connection@osns.net"
#define OSNS_SERVICE_SSH_REQUEST_FORWARD		"ssh-request-forward@osns.net"

/* message looks like:
    - uint32						length
    - byte						OSNS_MSG_SSH_REQUEST_FORWARD
    - ssh string					socket://path or tcpip4:// or tcpip6://
*/

/* open a channel for:
    - session -> subsystem sftp, exec, shell and or forward (tcpip4/6 and streamlocal) */

#define OSNS_MSG_SSH_CHANNEL_OPEN			90
#define OSNS_MSG_SSH_CHANNEL_OPEN_CONFIRMATION		91
#define OSNS_MSG_SSH_CHANNEL_OPEN_FAILURE		92
#define OSNS_MSG_SSH_CHANNEL_DATA			94
#define OSNS_MSG_SSH_CHANNEL_EXTENDED_DATA		95
#define OSNS_MSG_SSH_CHANNEL_EOF			96
#define OSNS_MSG_SSH_CHANNEL_CLOSE			97

#define OSNS_LOCALSOCKET_STATUS_VERSION			1
#define OSNS_LOCALSOCKET_STATUS_SESSION			2
#define OSNS_LOCALSOCKET_STATUS_PACKET			4
#define OSNS_LOCALSOCKET_STATUS_WAITING1		8
#define OSNS_LOCALSOCKET_STATUS_WAITING2		16
#define OSNS_LOCALSOCKET_STATUS_WAIT			( OSNS_LOCALSOCKET_STATUS_WAITING1 | OSNS_LOCALSOCKET_STATUS_WAITING2 )
#define OSNS_LOCALSOCKET_STATUS_DISCONNECT		32

struct osns_packet_s {
    uint32_t				len;
    unsigned char			type;
    uint32_t				seq;
    char				*buffer;
};

struct osns_localsocket_s {
    unsigned int			status;
    unsigned int			s_major;
    unsigned int			s_minor;
    unsigned int			c_major;
    unsigned int			c_minor;
    struct fs_connection_s		connection;
    pthread_mutex_t			mutex;
    pthread_cond_t			cond;
    unsigned char			threads;
    void				(* process_buffer)(struct osns_localsocket_s *l, struct osns_packet_s *p);
    unsigned int			size;
    unsigned int			read;
    char 				buffer[];
};

/* prototypes */

struct fs_connection_s *accept_client_connection_from_localsocket(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *s_conn);

#endif

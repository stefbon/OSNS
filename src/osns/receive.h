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

#ifndef OSNS_RECEIVE_H
#define OSNS_RECEIVE_H

#include "libosns-datatypes.h"
#include "libosns-socket.h"

#define OSNS_CMSG_BUFFERSIZE				32

#define OSNS_RECEIVE_STATUS_BUFFER			1
#define OSNS_RECEIVE_STATUS_PACKET			2
#define OSNS_RECEIVE_STATUS_WAITING1			4
#define OSNS_RECEIVE_STATUS_WAITING2			8
#define OSNS_RECEIVE_STATUS_WAIT			( OSNS_RECEIVE_STATUS_WAITING1 | OSNS_RECEIVE_STATUS_WAITING2 )
#define OSNS_RECEIVE_STATUS_DISCONNECT			16

#define OSNS_CONTROL_TYPE_FD				1

struct osns_control_s {
    unsigned char				type;
    union {
	int					fd;
    } data;
};

struct osns_receive_s {
    unsigned int				status;
    void					*ptr;
    struct shared_signal_s			*signal;
    void					(* process_data)(struct osns_receive_s *receive, char *data, unsigned int len, struct osns_control_s *ctrl);
    int						(* send)(struct osns_receive_s *receive, char *data, unsigned int len, int (* send_cb)(struct system_socket_s *sock, char *data, unsigned int size, void *ptr), void *ptr);
    struct msghdr				msg;
    char					cmsg_buffer[OSNS_CMSG_BUFFERSIZE];
    unsigned int				read;
    unsigned int				size;
    unsigned char				threads;
    char					*buffer;
};

#define OSNS_PACKET_STATUS_WAITING		(1 << 0)
#define OSNS_PACKET_STATUS_RESPONSE		(1 << 1)
#define OSNS_PACKET_STATUS_FINISH		(1 << 2)
#define OSNS_PACKET_STATUS_TIMEDOUT		(1 << 3)
#define OSNS_PACKET_STATUS_ERROR		(1 << 4)

#define OSNS_PACKET_STATUS_PERMANENT		(1 << 5)

struct osns_packet_s {
    unsigned int				status;
    struct list_element_s			list;
    struct osns_receive_s 			*r;
    uint32_t					id;
    void					*ptr;
    void					(* cb)(struct osns_packet_s *p, unsigned char type, char *d, unsigned int l, struct osns_control_s *ctrl);
    unsigned char				reply;
    uint32_t					size;
    char					*buffer;
};

/* prototypes */

void init_osns_packet(struct osns_packet_s *packet);
void osns_read_available_data(struct osns_receive_s *r);

#endif

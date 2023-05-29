/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_INTERFACE_INTERFACE_H
#define _LIB_INTERFACE_INTERFACE_H

#include "libosns-misc.h"
#include "libosns-network.h"
#include "libosns-datatypes.h"
#include "libosns-list.h"
#include "libosns-system.h"

#include "iocmd.h"

#define _INTERFACE_TYPE_FUSE				(1 << 0)
#define _INTERFACE_TYPE_NETWORK				(1 << 1)
#define _INTERFACE_TYPE_SSH_SESSION			(1 << 2)
#define _INTERFACE_TYPE_SSH_CHANNEL			(1 << 3)
#define _INTERFACE_TYPE_SFTP_CLIENT			(1 << 4)
#define _INTERFACE_TYPE_OSNS_CHANNEL			(1 << 5)

#define _INTERFACE_FLAG_BUFFER_INIT			1
#define _INTERFACE_FLAG_BUFFER_CLEAR			2

#define _INTERFACE_FLAG_SECONDARY_1TO1			4
#define _INTERFACE_FLAG_SECONDARY_1TON			8
#define _INTERFACE_FLAG_SECONDARY                       ( _INTERFACE_FLAG_SECONDARY_1TO1 | _INTERFACE_FLAG_SECONDARY_1TON )

#define _INTERFACE_FLAG_PRIMARY_1TO1			16
#define _INTERFACE_FLAG_PRIMARY_1TON			32
#define _INTERFACE_FLAG_PRIMARY                         ( _INTERFACE_FLAG_PRIMARY_1TON | _INTERFACE_FLAG_PRIMARY_1TO1 )

#define _INTERFACE_FLAG_CONNECT				64
#define _INTERFACE_FLAG_START				128
#define _INTERFACE_FLAG_ERROR                           256

#define _INTERFACE_BUFFER_FLAG_ALLOC			1
#define _INTERFACE_BUFFER_FLAG_ERROR			2

struct context_interface_s;

struct interface_list_s {
    int							type;
    char						*name;
    struct interface_ops_s 				*ops;
};

struct interface_ops_s {
    char						*name;
    unsigned int					(* populate)(struct context_interface_s *i, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start);
    unsigned int					(* get_buffer_size)(struct interface_list_s *ilist, struct context_interface_s *primary);
    int							(* init_buffer)(struct context_interface_s *i, struct interface_list_s *ilist, struct context_interface_s *primary);
    void						(* clear_buffer)(struct context_interface_s *i);
    struct list_element_s				list;
};

struct fuse_mount_s {
    unsigned char					type;
    unsigned int					maxread;
    void						*ptr;
};

struct sftp_target_s {
    unsigned int					flags;
    char						*prefix;
};

union interface_target_u {
    struct host_address_s 				*host;
    struct ip_address_s 				*ip;
    struct fuse_mount_s					*fuse;
    struct sftp_target_s				*sftp;
    char						*uri;
    char						*name;
    void						*ptr;
};

union interface_parameters_u {
    struct network_port_s				*port;
    unsigned int					value;
    void                                                *ptr;
};

#define INTERFACE_CTX_SIGNAL_TYPE_APPLICATION		1
#define INTERFACE_CTX_SIGNAL_TYPE_SSH_CHANNEL		2
#define INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION		3
#define INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE		4
#define INTERFACE_CTX_SIGNAL_TYPE_REMOTE		5

struct context_iocmd_s {
    int							(* in)(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type);
    int							(* out)(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type);
};

#define INTERFACE_STATUS_FLAG_NONE                      0
#define INTERFACE_STATUS_FLAG_CONNECTED                 1
#define INTERFACE_STATUS_FLAG_DISCONNECTED              2
#define INTERFACE_STATUS_FLAG_RECOVERABLE               4
#define INTERFACE_STATUS_FLAG_ERROR                     8
#define INTERFACE_STATUS_FLAG_TIMESPEC_AVAIL            16
#define INTERFACE_STATUS_FLAG_STANDALONE                32

struct interface_status_s {
    unsigned int                                        flags;
    unsigned int                                        errcode;
    struct system_timespec_s                            time;
};

struct context_interface_s {
    unsigned char					type;
    unsigned int					flags;
    unsigned int					unique;
    void						*ptr;
    int 						(* connect)(struct context_interface_s *i, union interface_target_u *t, union interface_parameters_u *p);
    int							(* start)(struct context_interface_s *i);
    char						*(* get_interface_buffer)(struct context_interface_s *i);
    struct list_header_s				*(* get_header_connections)(struct context_interface_s *i);
    unsigned int                                        (* get_interface_status)(struct context_interface_s *i, struct interface_status_s *status);
    void                                                (* set_primary)(struct context_interface_s *i, struct context_interface_s *p);
    void                                                (* set_secondary)(struct context_interface_s *i, struct context_interface_s *s);
    struct context_interface_s                          *(* get_primary)(struct context_interface_s *i);
    struct context_iocmd_s 				iocmd;
    struct interface_link_s {
	struct context_interface_s			*primary;
	union _secondary_link_u {
	    unsigned int				refcount;
	    struct context_interface_s			*interface;
	} secondary;
    } link;
    unsigned int					size;
    char						buffer[];
};

/* prototypes */

void init_context_interface(struct context_interface_s *i, unsigned int type, unsigned int size);
void reset_context_interface(struct context_interface_s *interface);
struct context_interface_s *get_primary_context_interface(struct context_interface_s *i);
void init_context_interfaces();
void init_interface_status(struct interface_status_s *istatus);

#endif

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

#ifndef _LIB_INTERFACE_WORKSPACE_H
#define _LIB_INTERFACE_WORKSPACE_H

#include "misc.h"
#include "network.h"

#define _INTERFACE_TYPE_SSH_SESSION			1
#define _INTERFACE_TYPE_SSH_CHANNEL			2
#define _INTERFACE_TYPE_FUSE				3
#define _INTERFACE_TYPE_SFTP				4
#define _INTERFACE_TYPE_SSH_SERVER			5

#define _SERVICE_TYPE_FUSE				1
#define _SERVICE_TYPE_PORT				2
#define _SERVICE_TYPE_SSH_CHANNEL			3
#define _SERVICE_TYPE_SFTP_SUBSYSTEM			4
#define _SERVICE_TYPE_SFTP_CLIENT			5

#define _NETWORK_PORT_TCP				1
#define _NETWORK_PORT_UDP				2

#define _INTERFACE_BUFFER_FLAG_ALLOC			1
#define _INTERFACE_BUFFER_FLAG_ERROR			2

#define _INTERFACE_FLAG_BUFFER_INIT			1
#define _INTERFACE_FLAG_BUFFER_CLEAR			2
#define _INTERFACE_FLAG_SECONDARY			4
#define _INTERFACE_FLAG_SERVER				8

#define _INTERFACE_SFTP_FLAG_NEWREADDIR			1

#define _CTX_OPTION_TYPE_INT				1
#define _CTX_OPTION_TYPE_PCHAR				2
#define _CTX_OPTION_TYPE_PVOID				3
#define _CTX_OPTION_TYPE_BUFFER				4

#define _CTX_OPTION_FLAG_ERROR				1
#define _CTX_OPTION_FLAG_ALLOC				2
#define _CTX_OPTION_FLAG_NOTDEFINED			4
#define _CTX_OPTION_FLAG_DEFAULT			8

struct ctx_option_s {
    unsigned char					type;
    unsigned char					flags;
    union {
	unsigned int					integer;
	char						*name;
	void						*ptr;
	struct ctx_option_buffer_s {
	    char					*ptr;
	    unsigned int				size;
	    unsigned int				len;
	} buffer;
    } value;
    void						(* free)(struct ctx_option_s *o);
};

struct network_port_s {
    unsigned int					nr;
    unsigned char					type;
};

struct service_address_s {
    unsigned char					type;
    union {
	struct network_port_s				port;
	struct _fuse_mount_s {
	    char					*source;
	    char					*mountpoint;
	    char					*name;
	} fuse;
	struct _ssh_channel_service_s {
	    unsigned char				type;
	    char					*uri; 		/* address of socket or port on remote server, may be empty */
	    char					*name;
	} channel;
	struct _sftp_service_s {
	    char					*name;		/* name to be used in context like name of shared directory */
	    char					*uri;
	    char					*prefix; 	/* prefix on remote server, may be empty */
	} sftp;
    } target;
};

#define INTERFACE_BACKEND_SFTP_PREFIX_HOME					1
#define INTERFACE_BACKEND_SFTP_PREFIX_ROOT					2
#define INTERFACE_BACKEND_SFTP_PREFIX_CUSTOM					3

#define INTERFACE_BACKEND_SFTP_FLAG_STATFS_OPENSSH				1
#define INTERFACE_BACKEND_SFTP_FLAG_FSYNC_OPENSSH				2

struct context_interface_s {
    unsigned char					type;
    unsigned int					flags;
    int 						(* connect)(uid_t uid, struct context_interface_s *interface, struct host_address_s *address, struct service_address_s *service);
    int							(* start)(struct context_interface_s *interface, int fd, struct ctx_option_s *option);
    int							(* signal_context)(struct context_interface_s *interface, const char *what, struct ctx_option_s *option);
    int							(* signal_interface)(struct context_interface_s *interface, const char *what, struct ctx_option_s *option);
    char						*(* get_interface_buffer)(struct context_interface_s *interface);
    union interface_link_s {
	struct context_interface_s			*primary;
	unsigned int					refcount;
    } link;
    union context_backend_u {
	struct _sftp_subsystem_s {
	    unsigned int				flags;
	    unsigned int				fattr_supported;
	    int						(* complete_path)(struct context_interface_s *interface, char *buffer, struct pathinfo_s *pathinfo);
	    unsigned int				(* get_complete_pathlen)(struct context_interface_s *interface, unsigned int len);
	    struct prefix_s {
		unsigned char				type;
		char					*path;
		unsigned int				len;
	    } prefix;
	} sftp;
	struct _ssh_channel_s {
	    void					(* receive_data)(struct context_interface_s *interface, char **buffer, unsigned int size, uint32_t seq, unsigned int flags);
	    int						(* send_data)(struct context_interface_s *interface, char *data, unsigned int len, uint32_t *seq);
	} ssh_channel;
	struct _fusesocket_s {
	    struct beventloop_s 			*loop;
	} fuse;
    } backend;
    unsigned int					size;
    char						buffer[];
};

struct interface_list_s {
    int					type;
    char				*name;
    struct interface_ops_s 		*ops;
};

struct interface_ops_s {
    char				*name;
    unsigned int			(* populate)(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start);
    unsigned int			(* get_buffer_size)(struct interface_list_s *ilist);
    int					(* init_buffer)(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary);
    void				(* clear_buffer)(struct context_interface_s *interface);
    struct list_element_s		list;
};

/* prototypes */

void init_ctx_option(struct ctx_option_s *option, unsigned char type);
void set_ctx_option_free(struct ctx_option_s *option, const char *what);

int init_context_interface(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary);
void reset_context_interface(struct context_interface_s *interface);
void add_interface_ops(struct interface_ops_s *ops);
struct interface_ops_s *get_next_interface_ops(struct interface_ops_s *ops);
unsigned int build_interface_ops_list(struct context_interface_s *interface, struct interface_list_s *ilist, unsigned int start);
struct interface_list_s *get_interface_ops(struct interface_list_s *ailist, unsigned int count, int type);

#endif

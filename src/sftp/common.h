/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _SFTP_COMMON_H
#define _SFTP_COMMON_H

#include <sys/types.h>
#include <pwd.h>

#include "workspace-interface.h"
#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "sftp/attr-read.h"
#include "sftp/attr-write.h"

#define SFTP_SENDHASH_TABLE_SIZE_DEFAULT		64

#define SFTP_CLIENT_FLAG_READDIRPLUS			1
#define SFTP_CLIENT_FLAG_NEWREADDIR			2

#define SFTP_RECEIVE_FLAG_ALLOC				1
#define SFTP_RECEIVE_FLAG_ERROR				2

struct sftp_client_s;

struct sftp_header_s {
    unsigned char					type;
    uint32_t						id;
    uint32_t						seq;
    unsigned int 					len;
    char						*buffer;
};

struct sftp_openmode_s {
    uint32_t						access;
    uint32_t						flags;
};

/* interface specific data like prefix */

struct sftp_send_ops_s {
    unsigned int					version;
    int							(* init)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* open)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* create)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* read)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* write)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* close)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* stat)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* lstat)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* fstat)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* setstat)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* fsetstat)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* realpath)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* readlink)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* opendir)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* readdir)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* remove)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* rmdir)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* mkdir)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* rename)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* symlink)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* block)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* unblock)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* extension)(struct sftp_client_s *sftp, struct sftp_request_s *r);
    int							(* custom)(struct sftp_client_s *sftp, struct sftp_request_s *r);
};

struct sftp_recv_ops_s {
    void						(* status)(struct sftp_client_s *sftp, struct sftp_header_s *header);
    void						(* handle)(struct sftp_client_s *sftp, struct sftp_header_s *header);
    void						(* data)(struct sftp_client_s *sftp, struct sftp_header_s *header);
    void						(* name)(struct sftp_client_s *sftp, struct sftp_header_s *header);
    void						(* attr)(struct sftp_client_s *sftp, struct sftp_header_s *header);
    void						(* extension)(struct sftp_client_s *sftp, struct sftp_header_s *header);
    void						(* extension_reply)(struct sftp_client_s *sftp, struct sftp_header_s *header);
};

struct sftp_time_ops_s {
    void						(* correct_time_s2c)(struct sftp_client_s *sftp, struct system_timespec_s *time);
    void						(* correct_time_c2s)(struct sftp_client_s *sftp, struct system_timespec_s *time);
};

#define SFTP_EXTENSION_FLAG_SUPPORTED			1
#define SFTP_EXTENSION_FLAG_MAPPED			2

#define SFTP_EXTENSION_ADD_RESULT_CREATED		1
#define SFTP_EXTENSION_ADD_RESULT_EXIST			2
#define SFTP_EXTENSION_ADD_RESULT_NO_SUPP_SERVER	3
#define SFTP_EXTENSION_ADD_RESULT_NO_SUPP_CLIENT	4

#define SFTP_EXTENSION_ADD_RESULT_ERROR			20

#define SFTP_EXTENSION_FIND_RESULT_EXIST		1
#define SFTP_EXTENSION_FIND_RESULT_NO_SUPP_SERVER	2
#define SFTP_EXTENSION_FIND_RESULT_NOT_FOUND		4

struct sftp_protocol_extension_s;

struct sftp_protocol_extension_client_s {
    struct list_element_s				list;
    struct ssh_string_s					name;
    unsigned int					(* get_data_len)(struct sftp_client_s *s, struct sftp_protocol_extension_s *ext, struct sftp_request_s *r);
    unsigned int					(* fill_data)(struct sftp_client_s *s, struct sftp_protocol_extension_s *ext, struct sftp_request_s *r, char *data, unsigned int len);
    unsigned int					(* get_size_buffer)(struct sftp_client_s *s, struct ssh_string_s *name, struct ssh_string_s *data);
    void						(* init)(struct sftp_client_s *s, struct sftp_protocol_extension_s *ext);
};

struct sftp_protocol_extension_server_s {
    struct list_element_s				list;
    struct ssh_string_s					name;
    struct ssh_string_s					data;
};

struct sftp_protocol_extension_s {
    unsigned int					flags;
    struct sftp_protocol_extension_server_s		*esbs;
    struct sftp_protocol_extension_client_s		*esbc;
    unsigned char					code;
    int							(* send)(struct sftp_client_s *s, struct sftp_protocol_extension_s *e, struct sftp_request_s *r);
    unsigned int					size;
    char						buffer[];
};

struct sftp_extensions_s {
    struct sftp_protocol_extension_s			*aextensions;
    unsigned int					size;
    struct list_header_s				supported_server;
    struct list_header_s				supported_client;
};

struct sftp_supported_s {
    union {
	struct supp_v06_s {
	    unsigned char				init;
	    unsigned int				attribute_mask;
    	    unsigned int				attribute_bits;
	    unsigned int				open_flags;
	    unsigned int				access_mask;
	    unsigned int				max_read_size;
	    unsigned int				open_block_vector;
	    unsigned int				block_vector;
	    unsigned int				attrib_extension_count;
	    unsigned int				extension_count;
	} v06;
	struct supp_v05_s {
	    unsigned char				init;
	    unsigned int				attribute_mask;
    	    unsigned int				attribute_bits;
	    unsigned int				open_flags;
	    unsigned int				access_mask;
	    unsigned int				max_read_size;
	} v05;
    } version;
    unsigned int					stat_mask;
};

struct sftp_sendhash_s {
    uint32_t						id;
    pthread_mutex_t					mutex;
};

#define SFTP_CLIENT_FLAG_INIT				(1 << 0)
#define SFTP_CLIENT_FLAG_ALLOC				(1 << 1)
#define SFTP_CLIENT_FLAG_SENDDATA			(1 << 2)
#define SFTP_CLIENT_FLAG_SESSION			(1 << 3)

struct sftp_context_s {
    unsigned int					unique;
    void 						*ctx;
    void						*conn;
    int							(* signal_ctx2sftp)(struct sftp_client_s **s, const char *what, struct ctx_option_s *o);
    int							(* signal_sftp2ctx)(struct sftp_client_s *s, const char *what, struct ctx_option_s *o);
    int							(* signal_conn2sftp)(struct sftp_client_s *s, const char *what, struct ctx_option_s *o);
    int							(* signal_sftp2conn)(struct sftp_client_s *s, const char *what, struct ctx_option_s *o);
    int							(* send_data)(struct sftp_client_s *s, char *buffer, unsigned int size, uint32_t *seq, struct list_element_s *list);
    void						(* receive_data)(struct sftp_client_s *s, char **buffer, unsigned int size, uint32_t seq, unsigned int flags);
};

#define SFTP_SIGNAL_FLAG_MUTEX_ALLOC			(1 << 0)
#define SFTP_SIGNAL_FLAG_COND_ALLOC			(1 << 1)
#define SFTP_SIGNAL_FLAG_CONNECTED			(1 << 2)
#define SFTP_SIGNAL_FLAG_DISCONNECTING			(1 << 3)
#define SFTP_SIGNAL_FLAG_DISCONNECTED			(1 << 4)
#define SFTP_SIGNAL_FLAG_DISCONNECT			( SFTP_SIGNAL_FLAG_DISCONNECTED | SFTP_SIGNAL_FLAG_DISCONNECTING )
#define SFTP_SIGNAL_FLAG_CLEARING			(1 << 5)
#define SFTP_SIGNAL_FLAG_CLEARED			(1 << 6)
#define SFTP_SIGNAL_FLAG_CLEAR				( SFTP_SIGNAL_FLAG_CLEARED | SFTP_SIGNAL_FLAG_CLEARING )
#define SFTP_SIGNAL_FLAG_FREEING			(1 << 7)

struct sftp_signal_s {
    unsigned int					flags;
    struct common_signal_s				*signal;
    uint32_t						seq;
    struct timespec					seqset;
    unsigned char					seqtype;
    struct generic_error_s				error;
};

struct sftp_receive_s {
    void						(* receive_data)(struct sftp_client_s *sftp, char **p_buffer, unsigned int size, uint32_t seq, unsigned int flags);
};

struct sftp_protocol_s {
    unsigned char					version;
};

struct sftp_client_s {
    struct sftp_context_s				context;
    struct sftp_signal_s				signal;
    unsigned int					flags;
    pthread_mutex_t					mutex;
    struct sftp_receive_s				receive;
    struct attr_context_s				attrctx;
    unsigned int					refcount;
    struct sftp_protocol_s 				protocol;
    struct list_header_s				pending;
    struct sftp_sendhash_s				sendhash;
    struct sftp_send_ops_s				*send_ops;
    struct sftp_recv_ops_s				*recv_ops;
    struct sftp_time_ops_s				time_ops;
    struct sftp_supported_s				supported;
    struct sftp_extensions_s				extensions;
    struct net_idmapping_s				*mapping;
};

/* prototypes */

void get_sftp_request_timeout(struct sftp_client_s *sftp, struct system_timespec_s *timeout);
uint32_t get_sftp_request_id(struct sftp_client_s *sftp);

void clear_sftp_client(struct sftp_client_s *sftp);
void free_sftp_client(struct sftp_client_s **p_sftp);

struct sftp_client_s *create_sftp_client(struct generic_error_s *error);
int init_sftp_client(struct sftp_client_s *sftp, uid_t uid, struct net_idmapping_s *m);
int start_init_sftp_client(struct sftp_client_s *sftp);

unsigned int get_sftp_buffer_size();

#endif

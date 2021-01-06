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
#include "common-protocol.h"

#define SFTP_SENDHASH_TABLE_SIZE_DEFAULT		64

#define SFTP_CLIENT_FLAG_READDIRPLUS			1
#define SFTP_CLIENT_FLAG_NEWREADDIR			2

#define SFTP_RECEIVE_FLAG_ALLOC				1
#define SFTP_RECEIVE_FLAG_ERROR				2

struct sftp_client_s;
struct attr_version_s;

struct sftp_header_s {
    unsigned char					type;
    uint32_t						id;
    uint32_t						seq;
    unsigned int 					len;
    char						*buffer;
};

typedef void (* read_attr_cb)(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av, struct sftp_attr_s *attr);

struct _valid_attrcb_s {
	uint32_t					code;
	unsigned char					shift;
	read_attr_cb					cb[2];
	char						*name;
};

struct attr_version_s {
    uint32_t						valid;
    unsigned char					type;
    uint64_t						size;
    uint32_t						permissions;
    struct _valid_attrcb_s				*attrcb;
    struct _valid_attrcb_s				*ntimecb;
    union {
	struct attr_v03_s {
	    uint32_t					uid;
	    uint32_t					gid;
	    uint32_t					accesstime;
	    uint32_t					modifytime;
	} v03;
	struct attr_v46_s {
	    struct ssh_string_s				owner;
	    struct ssh_string_s				group;
	    int64_t					accesstime;
	    uint64_t					accesstime_n;
	    int64_t					createtime;
	    uint64_t					createtime_n;
	    int64_t					modifytime;
	    uint64_t					modifytime_n;
	    struct ssh_string_s				acl;
	    uint32_t					bits;
	    uint32_t					bits_valid;
	    int64_t					changetime;
	    uint64_t					changetime_n;
	    unsigned char				texthint;
	    struct ssh_string_s				mimetype;
	    uint32_t					link_count;
	    struct ssh_string_s				untranslated_name;
	    uint64_t					alloc_size;
	} v46;
    } version;
    uint32_t						extended_count;
    char						*extensions;
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

struct sftp_attr_ops_s {
    void 						(* read_attributes)(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);
    void 						(* write_attributes)(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);
    unsigned int					(* write_attributes_len)(struct sftp_client_s *sftp, struct sftp_attr_s *attr);
    void						(* read_name_response)(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct ssh_string_s *name);
    void						(* read_attr_response)(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);
    void						(* read_sftp_features)(struct sftp_client_s *sftp);
    unsigned int					(* get_attribute_mask)(struct sftp_client_s *sftp);
    int							(* get_attribute_info)(struct sftp_client_s *sftp, unsigned int valid, const char *what);
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
    void						(* correct_time_s2c)(struct sftp_client_s *sftp, struct timespec *time);
    void						(* correct_time_c2s)(struct sftp_client_s *sftp, struct timespec *time);
};

#define SFTP_EXTENSION_FLAG_SUPPORTED						1
#define SFTP_EXTENSION_FLAG_MAPPED						2
#define SFTP_EXTENSION_FLAG_CREATE						4
#define SFTP_EXTENSION_FLAG_OVERRIDE_DATA					8

struct sftp_protocolextension_s {
    unsigned int					flags;
    unsigned int					nr;
    unsigned char					mapped;
    int							(* send_extension)(struct sftp_client_s *sftp, struct sftp_protocolextension_s *e, struct sftp_request_s *sftp_r, unsigned int *error);
    struct ssh_string_s					name;
    struct ssh_string_s					data;
    void						*ptr;
    void						(* event_cb)(struct ssh_string_s *name, struct ssh_string_s *data, void *ptr, unsigned int event);
    struct list_element_s				list;
    char						buffer[];
};

struct sftp_extensions_s {
    unsigned int					count;
    unsigned char					mapped;
    struct sftp_protocolextension_s			*mapextension;
    struct sftp_protocolextension_s			*fsync;
    struct sftp_protocolextension_s			*statvfs;
    struct list_header_s				header;
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
    unsigned int					attr_supported;
};

struct sftp_user_s {
    union {
	struct ssh_string_s 				name;
	unsigned int					id;
    } remote;
    uid_t						uid;
};

struct sftp_group_s {
    union {
	struct ssh_string_s 				name;
	unsigned int					id;
    } remote;
    gid_t						gid;
};

#define _SFTP_USER_MAPPING_SHARED			1
#define _SFTP_USER_MAPPING_NONSHARED			2

#define _SFTP_USERMAP_LOCAL_GID				1
#define _SFTP_USERMAP_REMOTE_IDS			2
#define _SFTP_USERMAP_LOCAL_IDS_UNKNOWN			4

#define GETENT_TYPE_USER				1
#define GETENT_TYPE_GROUP				2

struct getent_fields_s {
    unsigned char					flags;
    struct ssh_string_s					getent;
    char 						*name;
    unsigned int					len;
    union {
	struct _user_s {
	    uid_t					uid;
	    gid_t					gid;
	    char 					*fullname;
	    char 					*home;
	} user;
	struct _group_s {
	    gid_t					gid;
	} group;
    } type;
};

struct sftp_usermapping_s {
    unsigned char					type;
    unsigned char					mapping;
    uid_t						uid;
    struct passwd					pwd;
    char						*buffer;
    unsigned int					size;
    struct getent_fields_s				remote_user;
    struct getent_fields_s				remote_group;
    uid_t						unknown_uid;
    gid_t						unknown_gid;
    void						(* get_local_uid)(struct sftp_client_s *sftp, struct sftp_user_s *user);
    void						(* get_local_gid)(struct sftp_client_s *sftp, struct sftp_group_s *group);
    void						(* get_remote_user)(struct sftp_client_s *sftp, struct sftp_user_s *user);
    void						(* get_remote_group)(struct sftp_client_s *sftp, struct sftp_group_s *group);
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
    int							(* send_data)(struct sftp_client_s *s, char *buffer, unsigned int size, uint32_t *seq);
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
    pthread_mutex_t					*mutex;
    pthread_cond_t					*cond;
    uint32_t						seq;
    struct timespec					seqset;
    unsigned char					seqtype;
    struct generic_error_s				error;
};

struct sftp_receive_s {
    void						(* receive_data)(struct sftp_client_s *sftp, char **p_buffer, unsigned int size, uint32_t seq, unsigned int flags);
};

struct sftp_client_s {
    struct sftp_context_s				context;
    struct sftp_signal_s				signal;
    unsigned int					flags;
    pthread_mutex_t					mutex;
    struct sftp_receive_s				receive;
    unsigned int					refcount;
    unsigned int 					server_version;
    struct sftp_sendhash_s				sendhash;
    struct sftp_send_ops_s				*send_ops;
    struct sftp_recv_ops_s				*recv_ops;
    struct sftp_attr_ops_s				attr_ops;
    struct sftp_time_ops_s				time_ops;
    struct sftp_supported_s				supported;
    struct sftp_extensions_s				extensions;
    struct sftp_usermapping_s				usermapping;
};

/* prototypes */

void get_sftp_request_timeout(struct sftp_client_s *sftp, struct timespec *timeout);
uint32_t get_sftp_request_id(struct sftp_client_s *sftp);

void clear_sftp_client(struct sftp_client_s *sftp);
void free_sftp_client(struct sftp_client_s **p_sftp);

struct sftp_client_s *create_sftp_client(struct generic_error_s *error);
int init_sftp_client(struct sftp_client_s *sftp, uid_t uid);
int start_init_sftp_client(struct sftp_client_s *sftp);

unsigned int get_sftp_features(struct sftp_client_s *sftp);
unsigned int get_sftp_buffer_size();

#endif

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

#ifndef _SSH_COMMON_H
#define _SSH_COMMON_H

#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pwd.h>

#include "libosns-interface.h"
#include "libosns-list.h"
#include "libosns-network.h"
#include "libosns-users.h"
#include "libosns-datatypes.h"
#include "libosns-lock.h"
#include "libosns-connection.h"
#include "libosns-ssh.h"

#include "ssh-pk.h"
#include "ssh-common-protocol.h"
#include "ssh-hash.h"

struct ssh_session_s;
struct ssh_connection_s;
struct ssh_channel_s;
struct channel_table_s;
struct ssh_payload_s;

// #include "ssh-channel.h"
// #include "ssh-pubkey.h"

#define SSH_GREETER_START			        "SSH-"
#define SSH_GREETER_TERMINATOR_CRLF		        1
#define SSH_GREETER_TERMINATOR_LF		        2

#define SSH_PACKET_HEADER_SIZE				4

struct ssh_packet_s {
    unsigned int 					len;
    unsigned int 					size;
    unsigned char					padding;
    unsigned int 					error;
    unsigned int					sequence;
    unsigned char					type;
    unsigned int					decrypted;
    char 						*buffer;
};

typedef void (* receive_msg_cb_t)(struct ssh_connection_s *connection, struct ssh_payload_s *p);
typedef int (* send_data_ssh_channel_t)(struct ssh_channel_s *channel, unsigned int len, char *data, uint32_t *seq);

#define SSH_CONFIG_FLAG_CORRECT_CLOCKSKEW		(1 << 0)
#define SSH_CONFIG_FLAG_SUPPORT_CERTIFICATES		(1 << 1)
#define SSH_CONFIG_FLAG_SUPPORT_EXT_INFO		(1 << 2)

#define SSH_CONFIG_MAX_PACKET_SIZE			32768
#define SSH_CONFIG_RECEIVE_BUFFER_SIZE			35000

#define SSH_CONFIG_DEFAULT_PORT				22

#define SSH_CONFIG_CONNECTION_EXPIRE			2
#define SSH_CONFIG_USERAUTH_EXPIRE			12

#define SSH_CONFIG_MAX_RECEIVING_THREADS		4
#define SSH_CONFIG_MAX_SENDING_THREADS			4

/* the db/file on this machine with trusted host keys
    like /etc/ssh/known_hosts and ~/.ssh/known_hosts for openssh */

#define SSH_CONFIG_TRUSTDB_NONE				0
#define SSH_CONFIG_TRUSTDB_OPENSSH			1

#define SSH_CONFIG_AUTH_PASSWORD			(1 << 0)
#define SSH_CONFIG_AUTH_PUBLICKEY			(1 << 1)
#define SSH_CONFIG_AUTH_HOSTKEY				(1 << 2)

struct ssh_config_s {
    unsigned int					flags;
    unsigned int					max_packet_size;
    unsigned int					max_receive_size;
    unsigned int					port;
    unsigned int					connection_expire;
    unsigned int					userauth_expire;
    unsigned char					max_receiving_threads;
    unsigned char					max_sending_threads;
    unsigned int					extensions;
    unsigned int					global_requests;
    unsigned int					trustdb;
    unsigned int					auth;
};

#define SSH_CHANNEL_FLAG_INIT				(1 << 0)
#define SSH_CHANNEL_FLAG_TABLE				(1 << 1)
#define SSH_CHANNEL_FLAG_OPEN				(1 << 2)
#define SSH_CHANNEL_FLAG_OPENFAILURE			(1 << 3)
#define SSH_CHANNEL_FLAG_SERVER_EOF		        (1 << 4)
#define SSH_CHANNEL_FLAG_SERVER_CLOSE			(1 << 5)
#define SSH_CHANNEL_FLAG_CLIENT_EOF			(1 << 6)
#define SSH_CHANNEL_FLAG_CLIENT_CLOSE			(1 << 7)
#define SSH_CHANNEL_FLAG_NODATA				( SSH_CHANNEL_FLAG_CLIENT_CLOSE | SSH_CHANNEL_FLAG_CLIENT_EOF | SSH_CHANNEL_FLAG_SERVER_CLOSE | SSH_CHANNEL_FLAG_SERVER_EOF )

#define SSH_CHANNEL_FLAG_SERVER_SIGNAL			(1 << 8)

#define SSH_CHANNEL_FLAG_SEND_DATA			(1 << 9)
#define SSH_CHANNEL_FLAG_RECV_DATA		        (1 << 10)
#define SSH_CHANNEL_FLAG_EXIT_SIGNAL			(1 << 11)
#define SSH_CHANNEL_FLAG_EXIT_STATUS			(1 << 12)
#define SSH_CHANNEL_FLAG_QUEUE_EOF                      (1 << 13)
#define SSH_CHANNEL_FLAG_QUEUE_CLOSE                    (1 << 14)
#define SSH_CHANNEL_FLAG_QUEUE_EXIT_STATUS              (1 << 15)
#define SSH_CHANNEL_FLAG_QUEUE_EXIT_SIGNAL              (1 << 16)

#define SSH_CHANNEL_FLAG_UDP				(1 << 20)
#define SSH_CHANNEL_FLAG_CONNECTION_REFCOUNT		(1 << 21)
#define SSH_CHANNEL_FLAG_ALLOCATED			(1 << 22)
#define SSH_CHANNEL_FLAG_OUTGOING_DATA			(1 << 23)
#define SSH_CHANNEL_FLAG_INCOMING_DATA			(1 << 24)

#define SSH_CHANNEL_EXIT_SIGNAL_ABRT			1
#define SSH_CHANNEL_EXIT_SIGNAL_ALRM			2
#define SSH_CHANNEL_EXIT_SIGNAL_FPE			3
#define SSH_CHANNEL_EXIT_SIGNAL_HUP			4
#define SSH_CHANNEL_EXIT_SIGNAL_ILL			5
#define SSH_CHANNEL_EXIT_SIGNAL_INT			6
#define SSH_CHANNEL_EXIT_SIGNAL_KILL			7
#define SSH_CHANNEL_EXIT_SIGNAL_PIPE			8
#define SSH_CHANNEL_EXIT_SIGNAL_QUIT			9
#define SSH_CHANNEL_EXIT_SIGNAL_SEGV			10
#define SSH_CHANNEL_EXIT_SIGNAL_TERM			11
#define SSH_CHANNEL_EXIT_SIGNAL_USR1			12
#define SSH_CHANNEL_EXIT_SIGNAL_USR2			13

#define SSH_CHANNEL_TYPE_SESSION			1
#define SSH_CHANNEL_TYPE_DIRECT				2
#define SSH_CHANNEL_TYPE_FORWARDED			3

/* possible types for a session channel */

#define SSH_CHANNEL_SESSION_TYPE_NOTSET                 0
#define SSH_CHANNEL_SESSION_TYPE_SHELL			1
#define SSH_CHANNEL_SESSION_TYPE_EXEC			2
#define SSH_CHANNEL_SESSION_TYPE_SUBSYSTEM		3

/* possible types for direct and forwarded channels */

#define SSH_CHANNEL_SOCKET_TYPE_STREAMLOCAL		1
#define SSH_CHANNEL_SOCKET_TYPE_TCPIP			2

#define SSH_CHANNEL_DIRECT_FLAG_STREAMLOCAL_OPENSSH_COM	1

#define SSH_CHANNEL_DATA_RECEIVE_FLAG_ERROR		1
#define SSH_CHANNEL_DATA_RECEIVE_FLAG_ALLOC		2

#define SSH_CHANNEL_SESSION_BUFFER_MAXLEN		128
#define SSH_CHANNEL_STREAMLOCAL_PATH_MAX		108

struct ssh_channel_ctx_s {
    uint64_t						unique;
    void 						*ctx;
    int							(* signal_ctx2channel)(struct ssh_channel_s **s, const char *what, struct io_option_s *o, unsigned int type);
    int							(* signal_channel2ctx)(struct ssh_channel_s *s, const char *what, struct io_option_s *o, unsigned int type);
};

#define SSH_CHANNEL_IOCB_RECV_MAXCOUNT                  4
#define SSH_CHANNEL_IOCB_RECV_DATA                      0
#define SSH_CHANNEL_IOCB_RECV_XDATA                     1
#define SSH_CHANNEL_IOCB_RECV_REQUEST                   2
#define SSH_CHANNEL_IOCB_RECV_OPEN                      3

typedef void (* io_ssh_channel_recv_msg_t)(struct ssh_channel_s *channel, struct ssh_payload_s **payload);

struct ssh_channel_s {
    struct ssh_connection_s 				*connection;
    struct ssh_session_s				*session;
    struct ssh_channel_ctx_s				context;
    struct shared_signal_s				*signal;
    unsigned char					type;
    unsigned int					flags;
    unsigned int					exit_signal;
    unsigned int					exit_status;
    unsigned int 					lcnr;
    unsigned int					rcnr;
    unsigned int					maxpacketsize;
    uint32_t						lwindowsize;
    void						(* process_incoming_bytes)(struct ssh_channel_s *c, unsigned int size);
    uint32_t						rwindowsize;
    void						(* process_outgoing_bytes)(struct ssh_channel_s *c, unsigned int size);
    struct list_element_s				list;
    struct payload_queue_s				queue;
    int                                                 (* start)(struct ssh_channel_s *c, struct ssh_channel_open_data_s *d);
    io_ssh_channel_recv_msg_t                           iocb[SSH_CHANNEL_IOCB_RECV_MAXCOUNT];
    void						(* close)(struct ssh_channel_s *c, unsigned int flags);
    union {
	struct _socket_s {
	    unsigned char				type;
	    unsigned int				flags;
	    union _socket_type_u {
		struct _socket_local_s {
		    char				path[SSH_CHANNEL_STREAMLOCAL_PATH_MAX];
		} local;
		struct _socket_tcpip_s {
		    struct host_address_s		address;
		    unsigned int			portnr;
		    struct ip_address_s			orig_ip;
		    unsigned int			orig_portnr;
		} tcpip;
	    } stype;
	} socket;
	struct _session_s {
	    unsigned char				type;
	    char					buffer[SSH_CHANNEL_SESSION_BUFFER_MAXLEN];
	} session;
    } target;
};

#define SSH_ALGO_TYPE_KEX				0
#define SSH_ALGO_TYPE_HOSTKEY				1
#define SSH_ALGO_TYPE_CIPHER_C2S			2
#define SSH_ALGO_TYPE_CIPHER_S2C			3
#define SSH_ALGO_TYPE_HMAC_C2S				4
#define SSH_ALGO_TYPE_HMAC_S2C				5
#define SSH_ALGO_TYPE_COMPRESS_C2S			6
#define SSH_ALGO_TYPE_COMPRESS_S2C			7
#define SSH_ALGO_TYPE_LANG_C2S				8
#define SSH_ALGO_TYPE_LANG_S2C				9

#define SSH_ALGO_TYPES_COUNT				10

/* for most algos's some are recommended (high order), some required (medium) and some optional (low) */

#define SSH_ALGO_ORDER_LOW				1
#define SSH_ALGO_ORDER_MEDIUM				2
#define SSH_ALGO_ORDER_HIGH				3

struct algo_list_s {
    int							type;
    unsigned int					order;
    char						*sshname;
    char						*libname;
    void						*ptr;
};

#define SSH_ALGO_HASH_SHA1_160				1
#define SSH_ALGO_HASH_SHA2_256				2
#define SSH_ALGO_HASH_SHA2_512				3

struct ssh_dh_s {
    struct ssh_string_s					modp;
    struct ssh_string_s					modg;
    void						(* free)(struct ssh_dh_s *dh);
    struct ssh_mpint_s					p;
    struct ssh_mpint_s					g;
    struct ssh_mpint_s					x;
    struct ssh_mpint_s					e;
    struct ssh_mpint_s					f;
    struct ssh_mpint_s					sharedkey;
};

struct ssh_ecdh_s {
    unsigned int					status;
    void						(* free)(struct ssh_ecdh_s *ecdh);
    struct ssh_string_s					skey_c;
    struct ssh_string_s					pkey_s;
    struct ssh_string_s					sharedkey;
};

struct ssh_keyex_s;

struct keyex_ops_s {
    char						*name;
    unsigned int					(* populate)(struct ssh_connection_s *c, struct keyex_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    int							(* init)(struct ssh_keyex_s *k, char *name);
    int							(* create_local_key)(struct ssh_keyex_s *k);
    void						(* msg_write_local_key)(struct msg_buffer_s *mb, struct ssh_keyex_s *k);
    void						(* msg_read_remote_key)(struct msg_buffer_s *mb, struct ssh_keyex_s *k);
    void						(* msg_write_remote_key)(struct msg_buffer_s *mb, struct ssh_keyex_s *k);
    int							(* calc_sharedkey)(struct ssh_keyex_s *k);
    void						(* msg_write_sharedkey)(struct msg_buffer_s *mb, struct ssh_keyex_s *k);
    void						(* free)(struct ssh_keyex_s *k);
    struct list_element_s				list;
};

#define SSH_KEYEX_FLAG_SERVER				1

struct ssh_keyex_s {
    unsigned char					flags;
    struct ssh_pkalgo_s					*algo;
    char 						digestname[32];
    struct keyex_ops_s					*ops;
    union {
	struct ssh_dh_s					dh;
	struct ssh_ecdh_s				ecdh;
    } method;
};

struct ssh_utils_s {
    uint64_t 						(* ntohll)(uint64_t value);
};

struct ssh_cryptoactor_s {
    struct list_element_s				list;
    struct system_timespec_s				created;
    unsigned int					kexctr;
    unsigned int					nr;
    void						(* queue)(struct ssh_cryptoactor_s *ca);
    void                                                (* free)(struct list_element_s *list);
};

struct ssh_decompressor_s {
    struct ssh_cryptoactor_s                            common;
    struct ssh_decompress_s				*decompress;
    struct ssh_payload_s                                *(* decompress_packet)(struct ssh_decompressor_s *d, struct ssh_packet_s *packet, unsigned int *error);
    void                                                (* clear)(struct ssh_decompressor_s *d);
    unsigned int					size;
    char						buffer[];
};

struct decompress_ops_s {
    char						*name;
    unsigned int					(* populate)(struct ssh_connection_s *c, struct decompress_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    unsigned int					(* get_handle_size)(struct ssh_decompress_s *decompress);
    int							(* init)(struct ssh_decompressor_s *d);
    struct list_element_s				list;
};

struct ssh_decompress_s {
    unsigned int					flags;
    char						name[64];
    unsigned int					count;
    unsigned int					max_count;
    struct list_header_s				header;
    struct decompress_ops_s				*ops;
};

struct ssh_decryptor_s {
    struct ssh_cryptoactor_s                            common;
    struct ssh_decrypt_s				*decrypt;
    int							(* verify_hmac_pre)(struct ssh_decryptor_s *d, struct ssh_packet_s *p);
    int							(* decrypt_length)(struct ssh_decryptor_s *d, struct ssh_packet_s *p, char *b, unsigned int l);
    int							(* decrypt_packet)(struct ssh_decryptor_s *d, struct ssh_packet_s *p);
    int							(* verify_hmac_post)(struct ssh_decryptor_s *d, struct ssh_packet_s *p);
    void                                                (* clear)(struct ssh_decryptor_s *d);
    unsigned int					cipher_blocksize;
    unsigned int					cipher_headersize;
    unsigned int					hmac_maclen;
    unsigned int					size;
    char						buffer[];
};

struct decrypt_ops_s {
    char						*name;
    unsigned int					(* populate_cipher)(struct ssh_connection_s *s, struct decrypt_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    unsigned int					(* populate_hmac)(struct ssh_connection_s *s, struct decrypt_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    unsigned int					(* get_handle_size)(struct ssh_decrypt_s *d);
    int							(* init)(struct ssh_decryptor_s *decryptor);
    unsigned int					(* get_cipher_blocksize)(const char *name);
    unsigned int					(* get_cipher_keysize)(const char *name);
    unsigned int					(* get_cipher_ivsize)(const char *name);
    unsigned int					(* get_hmac_keysize)(const char *name);
    unsigned int					(* get_decrypt_flag)(const char *ciphername, const char *hmacname, const char *what);
    struct list_element_s				list;
};

#define SSH_DECRYPT_FLAG_PARALLEL			(1 << 1)

struct ssh_decrypt_s {
    unsigned int					flags;
    char 						ciphername[64];
    char						hmacname[64];
    unsigned int					count;			/* total number decryptors in use */
    unsigned int					max_count;		/* maximum number of decryptors, when set to 1 parallel is disabled */
    struct list_header_s				header;			/* linked list of decryptors (possibly one) */
    struct decrypt_ops_s				*ops;			/* decrypt ops used */
    struct ssh_string_s					cipher_key;
    struct ssh_string_s					cipher_iv;
    struct ssh_string_s					hmac_key;
};

/* receive in init phase y/n */

#define SSH_RECEIVE_STATUS_INIT				(1 << 0)

/* greeter received y/n */

#define SSH_RECEIVE_STATUS_GREETER			(1 << 1)

/* in session phase y/n */

#define SSH_RECEIVE_STATUS_SESSION			(1 << 2)

/* in kexinit phase */

#define SSH_RECEIVE_STATUS_KEXINIT			(1 << 3)

/* new keys received */

#define SSH_RECEIVE_STATUS_NEWKEYS			(1 << 4)

/* receive process locked */

#define SSH_RECEIVE_STATUS_LOCK				(1 << 5)

#define SSH_RECEIVE_STATUS_BUFFER			(1 << 6)
#define SSH_RECEIVE_STATUS_PROCESS			(1 << 7)
#define SSH_RECEIVE_STATUS_THREAD			(1 << 8)

/* is the receiving done in serial y/n (alternative is parallel) */

#define SSH_RECEIVE_STATUS_SERIAL			(1 << 10)
#define SSH_RECEIVE_STATUS_REKEY			(1 << 11)

#define SSH_RECEIVE_STATUS_ERROR			(1 << 30)
#define SSH_RECEIVE_STATUS_DISCONNECT			(1 << 31)

struct ssh_recv_sequence_error_s {
    unsigned int					sequence_number_error;
    unsigned int					errcode;
};

struct ssh_receive_s {
    unsigned int					status;
    struct system_timespec_s				newkeys;
    struct system_timespec_s				kexinit;
    unsigned int					kexctr;
    struct shared_signal_s				signal;			/* shared signal for receiving ssh processes only : transport level */
    struct ssh_decrypt_s				decrypt;
    struct ssh_decompress_s				decompress;
    unsigned int					threads;
    unsigned int 					sequence_number;
    struct ssh_recv_sequence_error_s			sequence_error;
    unsigned int                                        msgsize;
    unsigned int                                        read;
    unsigned int                                        size;
    char                                                *buffer;
};

/*
    struct for the sending site
				    */

struct ssh_encrypt_s;
struct ssh_compress_s;

/* struct to do the compressing of the payload
    in the buffer handles and backend library data are 
    the backend library determines how this looks like */

struct ssh_compressor_s {
    struct ssh_cryptoactor_s                            common;
    struct ssh_compress_s				*compress;
    int							(* compress_payload)(struct ssh_compressor_s *c, struct ssh_payload_s **payload, unsigned int *error);
    void                                                (* clear)(struct ssh_compressor_s *d);
    unsigned int					size;
    char						buffer[];
};

/* struct with calls to:
    - populate this compressor to the kexinit list
    - get the size to allocate before initializing
    - initialize the compressor*/

struct compress_ops_s {
    char						*name;
    unsigned int					(* populate)(struct ssh_connection_s *c, struct compress_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    unsigned int					(* get_handle_size)(struct ssh_compress_s *compress);
    int							(* init)(struct ssh_compressor_s *d);
    struct list_element_s				list;
};

/* holds the waiters list with compressors, and the compress_ops used at that time to create a new compressor */

struct ssh_compress_s {
    unsigned int					flags;
    char						name[64];
    unsigned int					count;
    unsigned int					max_count;
    struct list_header_s				header;
    struct compress_ops_s				*ops;
};

/* struct to do the encryption (is a cipher and a digest):
    - write hmac pre when pre encryption digest is used
    - encrypt the packet
    - write hmac post when post encryption digest is used
    - get message padding determines the padding of the message
    - clear clears the encrypt data in buffer */

struct ssh_encryptor_s {
    struct ssh_cryptoactor_s                            common;
    struct ssh_encrypt_s				*encrypt;
    int							(* write_hmac_pre)(struct ssh_encryptor_s *e, struct ssh_packet_s *packet);
    int							(* encrypt_packet)(struct ssh_encryptor_s *e, struct ssh_packet_s *packet);
    int							(* write_hmac_post)(struct ssh_encryptor_s *e, struct ssh_packet_s *packet);
    unsigned char					(* get_message_padding)(struct ssh_encryptor_s *e, unsigned int len);
    void                                                (* clear)(struct ssh_encryptor_s *d);
    unsigned int					cipher_blocksize;
    unsigned int					cipher_headersize;
    unsigned int					hmac_maclen;
    unsigned int					size;
    char						buffer[];
};

/* struct to
    - populate this encryptor (cipher and hmac) to the kexinit list
    - get the size to allocate before initializing
    - initialize the encryptor
    - get various sizes required of keys/iv's to allocate buffers*/

struct encrypt_ops_s {
    char						*name;
    unsigned int					(* populate_cipher)(struct ssh_connection_s *c, struct encrypt_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    unsigned int					(* populate_hmac)(struct ssh_connection_s *s, struct encrypt_ops_s *ops, struct algo_list_s *alist, unsigned int start);
    unsigned int					(* get_handle_size)(struct ssh_encrypt_s *encrypt);
    int							(* init)(struct ssh_encryptor_s *encryptor);
    unsigned int					(* get_cipher_blocksize)(const char *name);
    unsigned int					(* get_cipher_keysize)(const char *name);
    unsigned int					(* get_cipher_ivsize)(const char *name);
    unsigned int					(* get_hmac_keysize)(const char *name);
    unsigned int					(* get_encrypt_flag)(const char *ciphername, const char *hmacname, const char *what);
    struct list_element_s				list;
};

/* holds the waiters list with encryptors, and the ops to create a new encryptor
    additional key data*/

struct ssh_encrypt_s {
    unsigned int					flags;
    char						ciphername[64];
    char						hmacname[64];
    unsigned int					count;
    unsigned int					max_count;
    struct list_header_s				header;				/* linked list of encryptors (possibly one) */
    struct encrypt_ops_s				*ops;				/* encrypt ops used */
    struct ssh_string_s					cipher_key;
    struct ssh_string_s					cipher_iv;
    struct ssh_string_s					hmac_key;
};

struct ssh_send_s;

struct ssh_sender_s {
    struct list_element_s				list;
    unsigned int					sequence;
    unsigned char					type;
    int							(* queue_sender)(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error);
    void						(* pre_send)(struct ssh_send_s *send, struct ssh_sender_s *sender);
    void						(* post_send)(struct ssh_send_s *send, struct ssh_sender_s *sender, int bytessend);
};

#define SSH_SEND_FLAG_SESSION				(1 << 0)
#define SSH_SEND_FLAG_SERIAL				(1 << 1)
#define SSH_SEND_FLAG_LOCK				(1 << 2)
#define SSH_SEND_FLAG_ERROR				(1 << 3)
#define SSH_SEND_FLAG_DISCONNECT			(1 << 4)

struct ssh_send_s {
    unsigned int					flags;
    struct shared_signal_s				signal;
    struct list_header_s				senders;
    struct system_timespec_s				newkeys;
    unsigned int					kexctr;
    void 						(* set_sender)(struct ssh_send_s *send, struct ssh_sender_s *sender);
    unsigned int 					sequence_number;
    struct ssh_encrypt_s				encrypt;
    struct ssh_compress_s				compress;
};

struct ssh_pubkey_s {
    unsigned int 					pkalgo_client;
    unsigned int 					pkalgo_server;
    unsigned int					pksign_client;
    unsigned int					pksign_server;
    unsigned int					pkcert_client;
    unsigned int					pkcert_server;
};

#define SSH_HOSTINFO_FLAG_TIMEINIT			1
#define SSH_HOSTINFO_FLAG_TIMESET			2

struct ssh_hostinfo_s {
    unsigned int					flags;
    struct ssh_string_s 				fp;
    struct system_timespec_s				delta;
    void						(* correct_time_s2c)(struct ssh_session_s *session, struct system_timespec_s *time);
    void						(* correct_time_c2s)(struct ssh_session_s *session, struct system_timespec_s *time);
    struct net_idmapping_s				mapping;
};

#define SSH_IDENTITY_FLAG_VALID				1

struct ssh_identity_s {
    struct passwd					pwd;
    char						*buffer;
    unsigned int					size;
    struct ssh_string_s					remote_user;
    char						*identity_file;
};

/*	data per session:
	- greeters send by server and client
	- sessionid (H) calculated during kexinit in setup phase
*/

struct session_data_s {
    unsigned int 					remote_version_major;
    unsigned int 					remote_version_minor;
    struct ssh_string_s					sessionid;
    struct ssh_string_s					greeter_server;
    struct ssh_string_s					greeter_client;
};

#define SSH_ERROR_TYPE_SYSTEMERROR			1
#define SSH_ERROR_TYPE_INTERNAL				2
#define SSH_ERROR_TYPE_LIBRARY				3

struct ssh_error_s {
    unsigned int					type;
    char						description[128];
    union {
	unsigned int					systemerror;
    } error;
};

#define SSH_GREETER_FLAG_C2S				1
#define SSH_GREETER_FLAG_S2C				2

struct ssh_greeter_s {
    unsigned int					flags;
};

#define SSH_KEX_STATUS_OK				(1 << 0)
#define SSH_KEX_STATUS_ERROR				(1 << 1)

#define SSH_KEX_STATUS_KEXINIT				(1 << 2)
#define SSH_KEX_STATUS_KEX				(1 << 3)
#define SSH_KEX_STATUS_NEWKEYS				(1 << 4)
#define SSH_KEX_STATUS_COMPLETED			(1 << 5)

#define SSH_KEX_FLAG_KEXINIT_C2S			(1 << 0)
#define SSH_KEX_FLAG_KEXINIT_S2C			(1 << 1)
#define SSH_KEX_FLAG_KEXDH_C2S				(1 << 2)
#define SSH_KEX_FLAG_KEXDH_S2C				(1 << 3)
#define SSH_KEX_FLAG_NEWKEYS_C2S			(1 << 4)
#define SSH_KEX_FLAG_NEWKEYS_S2C			(1 << 5)
#define SSH_KEX_FLAG_COMPLETED				(1 << 6)

/*	data per keyexchange
	- algos available
	- kexinit messages server and client
	- algos chosen
	- keys and iv generated
*/

struct ssh_keyexchange_s {
    unsigned int					status;
    unsigned int					flags;
    struct ssh_string_s					kexinit_server;
    struct ssh_string_s					kexinit_client;
    struct algo_list_s					*algos;
    int							chosen[SSH_ALGO_TYPES_COUNT];
    struct ssh_string_s					cipher_key_c2s;
    struct ssh_string_s					cipher_iv_c2s;
    struct ssh_string_s					hmac_key_c2s;
    struct ssh_string_s					cipher_key_s2c;
    struct ssh_string_s					cipher_iv_s2c;
    struct ssh_string_s					hmac_key_s2c;
};

#define SSH_AUTH_METHOD_NONE				1
#define SSH_AUTH_METHOD_PUBLICKEY			2
#define SSH_AUTH_METHOD_PASSWORD			4
#define SSH_AUTH_METHOD_HOSTBASED			8
#define SSH_AUTH_METHOD_UNKNOWN				16

struct ssh_auth_s {
    unsigned int					required;
    unsigned int					done;
    struct ssh_string_s					c_hostname;
    struct ssh_string_s					c_ip;
    struct ssh_string_s					c_username;
    struct ssh_string_s					s_username;
};

#define SSH_TRANSPORT_TYPE_GREETER			1
#define SSH_TRANSPORT_TYPE_KEX				2

struct ssh_transport_s {
    unsigned int					status;
    union {
	struct ssh_keyexchange_s			kex;
	struct ssh_greeter_s				greeter;
    } type;
};

#define SSH_SERVICE_TYPE_AUTH				1
#define SSH_SERVICE_TYPE_CONNECTION			2

struct ssh_service_s {
    unsigned int					status;
    union {
	struct ssh_auth_s				auth;
    } type;
};

#define SSH_SESSION_COMPLETE_TYPE_KEX			1

struct ssh_session_complete_s {
    unsigned int					status;
    union {
	struct ssh_keyexchange_s			kex;
    } type;
};

#define SSH_SETUP_PHASE_TRANSPORT			1
#define SSH_SETUP_PHASE_SERVICE				2
#define SSH_SETUP_PHASE_SESSION				3

/* transport: greeter phase */
#define SSH_SETUP_FLAG_GREETER				(1 << 0)
/* transport: kex phase success (also rekex) */
#define SSH_SETUP_FLAG_KEX				(1 << 1)
/* transport */
#define SSH_SETUP_FLAG_TRANSPORT			(1 << 2)

/* service authentication success */
#define SSH_SETUP_FLAG_SERVICE_AUTH			(1 << 3)
/* service connection */
#define SSH_SETUP_FLAG_SERVICE_CONNECTION		(1 << 4)
/* session */
#define SSH_SETUP_FLAG_SESSION				(1 << 5)

/* connection: rekey exchange */
#define SSH_SETUP_FLAG_REKEX				(1 << 6)

#define SSH_SETUP_FLAG_RECV_ERROR			(1 << 24)
#define SSH_SETUP_FLAG_SEND_ERROR			(1 << 25)
#define SSH_SETUP_FLAG_RECV_EMPTY			(1 << 26)
#define SSH_SETUP_FLAG_SETUPTHREAD			(1 << 27)
#define SSH_SETUP_FLAG_HOSTINFO				(1 << 28)
#define SSH_SETUP_FLAG_ANALYZETHREAD			(1 << 29)
#define SSH_SETUP_FLAG_DISCONNECTING			(1 << 30)
#define SSH_SETUP_FLAG_DISCONNECTED			(1 << 31)
#define SSH_SETUP_FLAG_DISCONNECT			( SSH_SETUP_FLAG_DISCONNECTING | SSH_SETUP_FLAG_DISCONNECTED )

#define SSH_SETUP_OPTION_XOR				1
#define SSH_SETUP_OPTION_UNDO				2

struct ssh_setup_s {
    unsigned int					status;
    unsigned int 					flags;
    unsigned int					error;
    union {
	struct ssh_transport_s				transport;
	struct ssh_service_s				service;
    } phase;
    struct shared_signal_s				*signal;
    pthread_t						thread;
    struct payload_queue_s 				queue;
};

#define SSH_EXTENSION_SOURCE_EXT_INFO			1

#define SSH_EXTENSION_SERVER_SIG_ALGS			1
#define SSH_EXTENSION_DELAY_COMPRESSION			2
#define SSH_EXTENSION_NO_FLOW_CONTROL			3
#define SSH_EXTENSION_ELEVATION				4
#define SSH_EXTENSION_GR_SUPPORT			5

#define SSH_EXTENSIONS_COUNT				5

#define SSH_GLOBAL_REQUEST_ENUM_SUPPORTED		1
#define SSH_GLOBAL_REQUEST_ENUM_SERVICES		2
#define SSH_GLOBAL_REQUEST_INFO_SERVICE			3
#define SSH_GLOBAL_REQUEST_INFO_COMMAND			4
#define SSH_GLOBAL_REQUEST_UDP_CHANNEL			5
#define SSH_GLOBAL_REQUEST_TCPIP_FORWARD		6
#define SSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD		7
#define SSH_GLOBAL_REQUEST_STREAMLOCAL_FORWARD		8
#define SSH_GLOBAL_REQUEST_CANCEL_STREAMLOCAL_FORWARD	9

struct ssh_extension_s {
    char						*name;
    unsigned int					code;
};

struct ssh_extensions_s {
    unsigned int					supported;
    unsigned int					received;
    unsigned int					global_requests;
};

#define SSH_CONNECTION_FLAG_MAIN			1
#define SSH_CONNECTION_FLAG_DISCONNECT_SEND		2
/* global request pending */
#define SSH_CONNECTION_FLAG_GLOBAL_REQUEST		4
#define SSH_CONNECTION_FLAG_TROUBLE			8
#define SSH_CONNECTION_FLAG_DISCONNECTED		16

struct ssh_connection_s {
    unsigned char					unique;
    unsigned char					flags;
    unsigned char					refcount;
    receive_msg_cb_t					cb[256];
    struct ssh_setup_s					setup;
    struct connection_s					connection;
    struct ssh_receive_s				receive;
    struct ssh_send_s					send;
    void						(* pre_process)(struct ssh_connection_s *s);
    void						(* post_process_01)(struct ssh_connection_s *s);
    unsigned int					(* post_process_02)(struct ssh_connection_s *s, unsigned char type);
};

#define SSH_CONNECTIONS_FLAG_SIGNAL_ALLOCATED		(1 << 0)
#define SSH_CONNECTIONS_FLAG_LOCKED			(1 << 1)
#define SSH_CONNECTIONS_FLAG_DISCONNECTING		(1 << 20)
#define SSH_CONNECTIONS_FLAG_DISCONNECTED		(1 << 21)
#define SSH_CONNECTIONS_FLAG_DISCONNECT			( SSH_CONNECTIONS_FLAG_DISCONNECTED | SSH_CONNECTIONS_FLAG_DISCONNECTING )
#define SSH_CONNECTIONS_FLAG_CLEARING			(1 << 22)
#define SSH_CONNECTIONS_FLAG_CLEARED			(1 << 23)
#define SSH_CONNECTIONS_FLAG_CLEAR			( SSH_CONNECTIONS_FLAG_CLEARING | SSH_CONNECTIONS_FLAG_CLEARED )

struct ssh_connections_s {
    unsigned char					unique;
    unsigned int					flags;
    struct shared_signal_s				*signal;
    struct ssh_connection_s				*main;
    struct list_header_s				header;
};

#define SSH_CONTEXT_FLAG_SSH2REMOTE_GLOBAL_REQUEST	1
#define SSH_CONTEXT_FLAG_SSH2REMOTE_CHANNEL_EXEC	2
#define SSH_CONTEXT_FLAG_SSH2REMOTE_CHANNEL_SHELL	4
#define SSH_CONTEXT_FLAG_SSH2REMOTE_SUBSYSTEM		8

struct ssh_session_ctx_s {
    unsigned int					flags;
    uint64_t						unique;
    void 						*ctx;
    int							(* signal_ctx2ssh)(void **p_ptr, const char *what, struct io_option_s *o, unsigned int type);
    int							(* signal_ssh2ctx)(struct ssh_session_s *s, const char *what, struct io_option_s *o, unsigned int type);
    int							(* signal_ssh2remote)(struct ssh_session_s *s, const char *what, struct io_option_s *o, unsigned int type);
    // void                                                (* get_next_key)(struct ssh_session_s *s, unsigned int flags, struct ssh_key_s *key);
    // void                                                (* check_remote_host_key)(struct ssh_session_s *s, unsigned int flags, struct ssh_key_s *key);
};

/* main client session per user
    used for client and server */

#define SSH_SESSION_FLAG_LIST				1
#define SSH_SESSION_FLAG_INIT				2
#define SSH_SESSION_FLAG_SETUP				4
#define SSH_SESSION_FLAG_ALLOCATED			8
#define SSH_SESSION_FLAG_SERVER				16

#define SSH_SESSION_FLAG_CLEARING			32
#define SSH_SESSION_FLAG_CLEARED			64

#define SSH_SESSION_ALLFLAGS				( SSH_SESSION_FLAG_LIST | SSH_SESSION_FLAG_INIT | SSH_SESSION_FLAG_SETUP | SSH_SESSION_FLAG_ALLOCATED || SSH_SESSION_FLAG_SERVER )

struct ssh_session_s {
    unsigned int					flags;
    struct shared_signal_s				*signal;
    struct ssh_session_ctx_s				context;
    struct ssh_config_s					config;
    struct ssh_identity_s				identity;
    struct session_data_s				data;
    struct ssh_pubkey_s					pubkey;
    struct ssh_hostinfo_s				hostinfo;
    struct ssh_connections_s				connections;
    struct ssh_extensions_s				extensions;
    struct list_element_s				list;
};

#define SSH_SERVER_FLAG_ALLOC				1
#define SSH_SERVER_FLAG_INIT				2

struct ssh_server_s {
    unsigned int					flags;
    struct connection_s					listen;
    struct list_header_s				sessions;
};

/* prototypes */

void init_ssh_session_config(struct ssh_session_s *s);
int init_ssh_backend(struct shared_signal_s *signal);

void free_ssh_identity(struct ssh_session_s *s);
void init_ssh_identity(struct ssh_session_s *s);

void close_ssh_session(struct ssh_session_s *session);
void clear_ssh_session(struct ssh_session_s *s);
void free_ssh_session(void **p_ptr);

void disconnect_ssh_connection(struct ssh_connection_s *sshc);

struct ssh_session_s *create_ssh_session(unsigned int flags, struct generic_error_s *error);
int init_ssh_session(struct ssh_session_s *session, uid_t uid, void *ctx, struct shared_signal_s *signal);
int connect_ssh_session(struct ssh_session_s *session, char *target, unsigned int port);
int setup_ssh_session(struct ssh_session_s *session);

unsigned int get_window_size(struct ssh_session_s *session);
unsigned int get_max_packet_size(struct ssh_session_s *session);
void set_max_packet_size(struct ssh_session_s *session, unsigned int size);

int start_thread_ssh_connection_problem(struct ssh_connection_s *connection);

#endif

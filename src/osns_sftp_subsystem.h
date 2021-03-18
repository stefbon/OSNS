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

#ifndef OSNS_SFTP_SUBSYSTEM_H
#define OSNS_SFTP_SUBSYSTEM_H

#include <pwd.h>
#include "network.h"

#define SFTP_PROTOCOL_VERSION_DEFAULT		6

#define SFTP_ATTR_INDEX_SIZE			0
#define SFTP_ATTR_INDEX_USERGROUP		1
#define SFTP_ATTR_INDEX_PERMISSIONS		2
#define SFTP_ATTR_INDEX_ATIME			3
#define SFTP_ATTR_INDEX_ATIME_N			4
#define SFTP_ATTR_INDEX_MTIME			5
#define SFTP_ATTR_INDEX_MTIME_N			6
#define SFTP_ATTR_INDEX_CTIME			7
#define SFTP_ATTR_INDEX_CTIME_N			8

#define SSH_FILEXFER_ATTR_SIZE 			0x00000001
#define SSH_FILEXFER_INDEX_SIZE			0
#define SSH_FILEXFER_ATTR_UIDGID 		0x00000002
#define SSH_FILEXFER_INDEX_UIDGID		1
#define SSH_FILEXFER_ATTR_PERMISSIONS 		0x00000004
#define SSH_FILEXFER_INDEX_PERMISSIONS		2
#define SSH_FILEXFER_ATTR_ACCESSTIME 		0x00000008
#define SSH_FILEXFER_INDEX_ACCESSTIME		3
#define SSH_FILEXFER_ATTR_CREATETIME 		0x00000010
#define SSH_FILEXFER_INDEX_CREATETIME		4
#define SSH_FILEXFER_ATTR_MODIFYTIME 		0x00000020
#define SSH_FILEXFER_INDEX_MODIFYTIME		5
#define SSH_FILEXFER_ATTR_ACL 			0x00000040
#define SSH_FILEXFER_INDEX_ACL			6
#define SSH_FILEXFER_ATTR_OWNERGROUP 		0x00000080
#define SSH_FILEXFER_INDEX_OWNERGROUP		7
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES 	0x00000100
#define SSH_FILEXFER_INDEX_SUBSECOND_TIMES	8
#define SSH_FILEXFER_ATTR_BITS			0x00000200
#define SSH_FILEXFER_INDEX_BITS			9
#define SSH_FILEXFER_ATTR_ALLOCATION_SIZE	0x00000400
#define SSH_FILEXFER_INDEX_ALLOCATION_SIZE	10
#define SSH_FILEXFER_ATTR_TEXT_HINT		0x00000800
#define SSH_FILEXFER_INDEX_TEXT_HINT		11
#define SSH_FILEXFER_ATTR_MIME_TYPE		0x00001000
#define SSH_FILEXFER_INDEX_MIME_TYPE		12
#define SSH_FILEXFER_ATTR_LINK_COUNT		0x00002000
#define SSH_FILEXFER_INDEX_LINK_COUNT		13
#define SSH_FILEXFER_ATTR_UNTRANSLATED_NAME	0x00004000
#define SSH_FILEXFER_INDEX_UNTRANSLATED_NAME	14
#define SSH_FILEXFER_ATTR_CTIME			0x00008000
#define SSH_FILEXFER_INDEX_CTIME		15
#define SSH_FILEXFER_ATTR_EXTENDED	 	0x80000000
#define SSH_FILEXFER_INDEX_EXTENDED		31

#define SFTP_ATTR_FLAG_NOUSER			1
#define SFTP_ATTR_FLAG_NOGROUP			2
#define SFTP_ATTR_FLAG_USERNOTFOUND		4
#define SFTP_ATTR_FLAG_GROUPNOTFOUND		8
#define	SFTP_ATTR_FLAG_VALIDUSER		16
#define	SFTP_ATTR_FLAG_VALIDGROUP		32

/* file types */

#define SSH_FILEXFER_TYPE_REGULAR		1
#define SSH_FILEXFER_TYPE_DIRECTORY		2
#define SSH_FILEXFER_TYPE_SYMLINK		3
#define SSH_FILEXFER_TYPE_SPECIAL		4
#define SSH_FILEXFER_TYPE_UNKNOWN		5
#define SSH_FILEXFER_TYPE_SOCKET		6
#define SSH_FILEXFER_TYPE_CHAR_DEVICE		7
#define SSH_FILEXFER_TYPE_BLOCK_DEVICE		8
#define SSH_FILEXFER_TYPE_FIFO			9

/* error codes */

#define SSH_FX_OK 				0
#define SSH_FX_EOF 				1
#define SSH_FX_NO_SUCH_FILE 			2
#define SSH_FX_PERMISSION_DENIED 		3
#define SSH_FX_FAILURE 				4
#define SSH_FX_BAD_MESSAGE 			5
#define SSH_FX_NO_CONNECTION 			6
#define SSH_FX_CONNECTION_LOST 			7
#define SSH_FX_OP_UNSUPPORTED 			8
#define SSH_FX_INVALID_HANDLE 			9
#define SSH_FX_NO_SUCH_PATH 			10
#define SSH_FX_FILE_ALREADY_EXISTS 		11
#define SSH_FX_WRITE_PROTECT	 		12
#define SSH_FX_NO_MEDIA 			13
#define SSH_FX_NO_SPACE_ON_FILESYSTEM		14
#define SSH_FX_QUOTA_EXCEEDED			15
#define SSH_FX_UNKNOWN_PRINCIPAL		16
#define SSH_FX_LOCK_CONFLICT			17
#define SSH_FX_DIR_NOT_EMPTY			18
#define SSH_FX_NOT_A_DIRECTORY			19
#define SSH_FX_INVALID_FILENAME			20
#define SSH_FX_LINK_LOOP			21
#define SSH_FX_CANNOT_DELETE			22
#define SSH_FX_INVALID_PARAMETER		23
#define SSH_FX_FILE_IS_A_DIRECTORY		24
#define SSH_FX_BYTE_RANGE_LOCK_CONFLICT		25
#define SSH_FX_BYTE_RANGE_LOCK_REFUSED		26
#define SSH_FX_DELETE_PENDING			27
#define SSH_FX_FILE_CORRUPT			28
#define SSH_FX_OWNER_INVALID			29
#define SSH_FX_GROUP_INVALID			30
#define SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK	31

struct sftp_subsystem_s;
struct sftp_payload_s;

typedef void (* sftp_cb_t)(struct sftp_payload_s *p);

struct sftp_user_s {
    union {
	struct ssh_string_s 			name;
	unsigned int				id;
    } remote;
    uid_t					local_uid;
};

struct sftp_group_s {
    union {
	struct ssh_string_s 			name;
	unsigned int				id;
    } remote;
    gid_t					local_gid;
};

struct sftp_time_s {
    int64_t					sec;
    uint32_t					nsec;
};

struct sftp_attr_s {
    unsigned int 				flags;
    unsigned char				valid[9];
    unsigned int				count;
    unsigned int				type;
    uint64_t					size;
    uid_t					uid;
    struct ssh_string_s				user;
    gid_t					gid;
    struct ssh_string_s				group;
    uint32_t					permissions;
    struct sftp_time_s				atime;
    struct sftp_time_s				mtime;
    struct sftp_time_s				ctime;
};

#define SFTP_RECEIVE_STATUS_INIT			(1 << 0)
#define SFTP_RECEIVE_STATUS_PACKET			(1 << 10)
#define SFTP_RECEIVE_STATUS_WAITING1			(1 << 11)
#define SFTP_RECEIVE_STATUS_WAITING2			(1 << 12)
#define SFTP_RECEIVE_STATUS_WAIT			( SFTP_RECEIVE_STATUS_WAITING2 | SFTP_RECEIVE_STATUS_WAITING1 )
#define SFTP_RECEIVE_STATUS_ERROR			(1 << 30)
#define SFTP_RECEIVE_STATUS_DISCONNECT			(1 << 31)

#define SFTP_RECEIVE_BUFFER_SIZE_DEFAULT		16384

struct sftp_receive_s {
    unsigned char					flags;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
    void						(* process_sftp_payload)(struct sftp_payload_s *payload);
    unsigned int					read;
    unsigned int					size;
    unsigned char					threads;
    char						*buffer;
};

struct sftp_payload_s {
    struct sftp_subsystem_s				*sftp;
    unsigned int					len;
    unsigned char					type;
    uint32_t						id;
    struct list_element_s				list;
    char						data[];
};

struct sftp_payload_queue_s {
    struct list_header_s				header;
    unsigned int					threads;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
};

struct sftp_identity_s {
    struct passwd					pwd;
    unsigned int					size;
    unsigned int 					len_home;
    char						*buffer;
};

#define SFTP_SUBSYSTEM_FLAG_INIT			(1 << 0)
#define SFTP_SUBSYSTEM_FLAG_VERSION_RECEIVED		(1 << 1)
#define SFTP_SUBSYSTEM_FLAG_VERSION_SEND		(1 << 2)

#define SFTP_SUBSYSTEM_FLAG_FINISH			(1 << 27)
#define SFTP_SUBSYSTEM_FLAG_TROUBLE			(1 << 28)
#define SFTP_SUBSYSTEM_FLAG_DISCONNECTING		(1 << 30)
#define SFTP_SUBSYSTEM_FLAG_DISCONNECTED		(1 << 31)

#define SFTP_CONNECTION_FLAG_STD			(1 << 0)
#define SFTP_CONNECTION_FLAG_RECV_EMPTY			(1 << 1)
#define SFTP_CONNECTION_FLAG_RECV_ERROR			(1 << 2)


struct sftp_std_connection_s {
    struct fs_connection_s				stdin;
    struct fs_connection_s				stdout;
    struct fs_connection_s				stderr;
};

struct sftp_connection_s {
    unsigned char					flags;
    unsigned int					error;
    union {
	struct sftp_std_connection_s			std;
    } type;
    int							(* open)(struct sftp_connection_s *c);
    int							(* read)(struct sftp_connection_s *c, char *buffer, unsigned int size);
    int							(* write)(struct sftp_connection_s *c, char *data, unsigned int size);
    int							(* close)(struct sftp_connection_s *c);

    /* TODO:
	read_extended and write_extended for different data like error messages and other data */
};

/* TODO:
    struct ssh_subsystem_s {
	flags
	connection
	identity
	receive
	union {
	    sftp { queue, ..}

	}
    }
*/

struct sftp_subsystem_s {
    unsigned int					flags;
    unsigned int					version;
    struct sftp_connection_s				connection;
    struct sftp_identity_s				identity;
    struct sftp_receive_s				receive;
    struct sftp_payload_queue_s				queue;
    sftp_cb_t						cb[256];
};

/* prototypes */

#endif

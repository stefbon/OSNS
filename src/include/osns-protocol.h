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

#ifndef OSNS_PROTOCOL_H
#define OSNS_PROTOCOL_H

#include "lib/datatypes/name-string.h"
#include "lib/system/time.h"

/* new types:

    name string:
    byte						length
    bytes[length]					name

    record string
    2 bytes						length
    bytes[length]					record

*/

#define OSNS_COMMAND_TYPE_LIST				1
#define OSNS_COMMAND_TYPE_MOUNT				2
#define OSNS_COMMAND_TYPE_WATCH				3
#define OSNS_COMMAND_TYPE_CHANNEL			4
#define OSNS_COMMAND_TYPE_CONNECTIONS			5

#define OSNS_LIST_TYPE_MOUNTINFO			2
#define OSNS_LIST_TYPE_CONNECTIONS			3
#define OSNS_LIST_TYPE_BACKUP                           4

#define OSNS_MOUNT_TYPE_NETWORK				1
#define OSNS_MOUNT_TYPE_DEVICES				2

#define OSNS_WATCH_TYPE_FSNOTIFY			1
#define OSNS_WATCH_TYPE_MOUNTINFO			2
#define OSNS_WATCH_TYPE_NETCACHE			3
#define OSNS_WATCH_TYPE_CONNECTIONS			4

#define OSNS_CHANNEL_TYPE_EXEC				1
#define OSNS_CHANNEL_TYPE_SHELL				2
#define OSNS_CHANNEL_TYPE_SUBSYSTEM			3
#define OSNS_CHANNEL_TYPE_DIRECTIO			4
#define OSNS_CHANNEL_TYPE_READ                          5

#define OSNS_CONNECTION_TYPE_ADD			1

/* flags used at initialization to get/set the required and the provided abilities */

#define OSNS_INIT_FLAG_LIST_MOUNTINFO			(1 << 1)
#define OSNS_INIT_FLAG_MOUNT_NETWORK			(1 << 2)
#define OSNS_INIT_FLAG_MOUNT_DEVICES			(1 << 3)
#define OSNS_INIT_FLAG_WATCH_FSNOTIFY			(1 << 4)
#define OSNS_INIT_FLAG_DNSLOOKUP			(1 << 5)
#define OSNS_INIT_FLAG_FILTER_MOUNTINFO			(1 << 7)
#define OSNS_INIT_FLAG_WATCH_NETCACHE			(1 << 8)
#define OSNS_INIT_FLAG_LIST_CONNECTIONS			(1 << 9)
#define OSNS_INIT_FLAG_FILTER_CONNECTIONS		(1 << 10)
#define OSNS_INIT_FLAG_WATCH_MOUNTINFO			(1 << 11)
#define OSNS_INIT_FLAG_WATCH_CONNECTIONS		(1 << 12)
#define OSNS_INIT_FLAG_CHANNEL_EXEC			(1 << 13)
#define OSNS_INIT_FLAG_CHANNEL_SHELL			(1 << 14)
#define OSNS_INIT_FLAG_CHANNEL_SUBSYSTEM		(1 << 15)
#define OSNS_INIT_FLAG_CHANNEL_DIRECTIO			(1 << 16)
#define OSNS_INIT_FLAG_CONNECTION_ADD			(1 << 17)
#define OSNS_INIT_FLAG_LIST_BACKUP                      (1 << 18)
#define OSNS_INIT_FLAG_CHANNEL_READ			(1 << 19)

#define OSNS_INIT_FLAG_MOUNT				(OSNS_INIT_FLAG_MOUNT_DEVICES | OSNS_INIT_FLAG_MOUNT_NETWORK)
#define OSNS_INIT_FLAG_LIST				(OSNS_INIT_FLAG_LIST_MOUNTINFO | OSNS_INIT_FLAG_LIST_CONNECTIONS | OSNS_INIT_FLAG_LIST_BACKUP )
#define OSNS_INIT_FLAG_FILTER				(OSNS_INIT_FLAG_FILTER_MOUNTINFO | OSNS_INIT_FLAG_FILTER_CONNECTIONS)
#define OSNS_INIT_FLAG_WATCH				(OSNS_INIT_FLAG_WATCH_FSNOTIFY | OSNS_INIT_FLAG_WATCH_MOUNTINFO | OSNS_INIT_FLAG_WATCH_NETCACHE | OSNS_INIT_FLAG_WATCH_CONNECTIONS)
#define OSNS_INIT_FLAG_UTIL				OSNS_INIT_FLAG_DNSLOOKUP
#define OSNS_INIT_FLAG_CHANNEL				OSNS_INIT_FLAG_CHANNEL_EXEC | OSNS_INIT_FLAG_CHANNEL_READ

#define OSNS_INIT_ALL_FLAGS				(OSNS_INIT_FLAG_LIST | OSNS_INIT_FLAG_MOUNT | OSNS_INIT_FLAG_WATCH | OSNS_INIT_FLAG_UTIL | OSNS_INIT_FLAG_FILTER | OSNS_INIT_FLAG_CHANNEL)

struct osns_system_service_s {
    unsigned int					index;
    unsigned int					initflag;
    char						*name;
};

#define OSNS_DEFAULT_RUNPATH				"/run/osns"
#define OSNS_DEFAULT_UNIXGROUP				"osns"
#define OSNS_DEFAULT_NETWORK_MOUNT_PATH			"/run/network"
#define OSNS_DEFAULT_DEVICES_MOUNT_PATH			"/run/devices"
#define OSNS_DEFAULT_ETCPATH				"/etc/osns"
#define OSNS_DEFAULT_BACKUP_PATH                        "/var/lib/osns/backup"

#define OSNS_DEFAULT_FUSE_MAXREAD			8192

/* OSNS Message Codes */

#define OSNS_MSG_INIT					1
#define OSNS_MSG_VERSION				2
#define OSNS_MSG_DISCONNECT				3
#define OSNS_MSG_UNIMPLEMENTED				4

#define OSNS_MSG_OPENQUERY				10
#define OSNS_MSG_READQUERY				11
#define OSNS_MSG_CLOSEQUERY				12

#define OSNS_MSG_MOUNTCMD				22
#define OSNS_MSG_MOUNTED				23
#define OSNS_MSG_UMOUNTCMD				24

#define OSNS_MSG_CHANNEL_OPEN				30
#define OSNS_MSG_CHANNEL_OPEN_CONFIRMATION		31
#define OSNS_MSG_CHANNEL_OPEN_FAILURE			32
#define OSNS_MSG_CHANNEL_CLOSE				34
#define OSNS_MSG_CHANNEL_START                          35

#define OSNS_MSG_SETWATCH				40
#define OSNS_MSG_RMWATCH				41
#define OSNS_MSG_EVENT					42

#define OSNS_MSG_CONNECTION_ADD				50
#define OSNS_MSG_CONNECTION_SUCCESS                     51
#define OSNS_MSG_CONNECTION_FAILURE                     52

#define OSNS_MSG_BACKUP_OPEN                            60
#define OSNS_MSG_BACKUP_CONFIRMATION                    61
#define OSNS_MSG_BACKUP_FAILURE                         62
#define OSNS_MSG_BACKUP_CLOSE                           63

#define OSNS_MSG_STATUS					100
#define OSNS_MSG_NAME					101
#define OSNS_MSG_RECORDS				102
#define OSNS_MSG_MAX					102

#define OSNS_STATUS_OK					0
#define OSNS_STATUS_NOTSUPPORTED			1
#define OSNS_STATUS_SYSTEMERROR				2
#define OSNS_STATUS_HANDLENOTFOUND			3
#define OSNS_STATUS_PROTOCOLERROR			4
#define OSNS_STATUS_EOF					5
#define OSNS_STATUS_INVALIDFLAGS			6
#define OSNS_STATUS_INVALIDPARAMETERS			7
#define OSNS_STATUS_ALREADYMOUNTED			8
#define OSNS_STATUS_NOTFOUND				9
#define OSNS_STATUS_EXIST				10


#define OSNS_WATCH_EVENT_ADDED				1
#define OSNS_WATCH_EVENT_REMOVED			2
#define OSNS_WATCH_EVENT_CHANGED			3

#define OSNS_WATCH_EVENT_DOMAIN				(1 << 3)
#define OSNS_WATCH_EVENT_HOST				(1 << 4)
#define OSNS_WATCH_EVENT_SERVICE			(1 << 5)

/*
    OSNS_MSG_OPENQUERY
    byte						OSNS_MSG_OPENQUERY
    uint32						id
    uint8						what
    what:
							OSNS_LIST_TYPE_MOUNTINFO
							OSNS_LIST_TYPE_CONNECTIONS
							OSNS_LIST_TYPE_BACKUP

    uint32						flags
    uint32						valid

    where:
    - flags are various bits which have a meaining within the context of the name what
    - valid is a bit mask of what the client want to reply (==0 -> everything)

    reply with OSNS_MSG_STATUS (error) or OSNS_MSG_NAME
*/

/*
    OSNS_MSG_READQUERY
    byte						OSNS_MSG_READQUERY
    uint32						id
    name string						handle
    uint32						size
    uint32						offset

    reply with OSNS_MSG_STATUS (error) or OSNS_MSG_RECORDS
    how a record looks like depends on the type query
*/

/*
    OSNS_MSG_CLOSEQUERY
    byte						OSNS_MSG_CLOSEQUERY
    uint32						id
    name string						handle

    reply with OSNS_MSG_STATUS (ok)
*/

/*
    OSNS_MSG_MOUNTCMD
    byte                                                OSNS_MSG_MOUNTCMD
    uint32                                              id
    uint8                                               type
    uint32                                              maxread
*/

/*
    OSNS_MSG_SETWATCH
    byte						OSNS_MSG_SETWATCH
    uint32						id
    uint8						what
    what:						OSNS_WATCH_TYPE_FSNOTIFY
							OSNS_WATCH_TYPE_MOUNTINFO
							OSNS_WATCH_TYPE_NETCACHE
							OSNS_WATCH_TYPE_CONNECTIONS
    uint32                                              flags
    uint32                                              properties (like: mask)

    when what=="fsnotify"
    osns record						path

*/

struct osns_name_s {
    unsigned char			len;
    char				*str;
    char				data[];
};

struct osns_record_s {
    uint16_t				len;
    char				*data;
};


/*
    OSNS backup attr
    uint16                                              flags
    {int64,uint32}                                      time last update
    uint32                                              unique
*/

/*
    OSNS_MSG_BACKUP_OPEN
    byte                                                OSNS_MSG_BACKUP_OPEN
    uint32                                              id
    uint32                                              unique
*/

/*
    OSNS_MSG_BACKUP_OPEN_CONFIRMATION
    byte                                                OSNS_MSG_BACKUP_OPEN_CONFIRMATION
    uint32                                              id
    BACKUPATTR                                          attr
*/

/*
    OSNS_MSG_BACKUP_OPEN_FAILURE
    byte                                                OSNS_MSG_BACKUP_OPEN_FAILURE
    uint32                                              id
    uint8                                               code
*/

#define OSNS_BACKUP_FLAG_LOCALHOST                      1
#define OSNS_BACKUP_FLAG_USER                           2
#define OSNS_BACKUP_FLAG_SYSTEM                         4
#define OSNS_BACKUP_FLAG_AVAILABLE                      8
#define OSNS_BACKUP_FLAG_USED                           16
#define OSNS_BACKUP_FLAG_VERSIONS                       32

struct osns_backup_attr_s {
    uint32_t                                            flags;
    uint32_t                                            hostid;
    uint32_t                                            unique;
    uint32_t                                            offset;
    uint8_t                                             len;
    char                                                name[];
};

#define OSNS_BACKUP_VALID_CHANGED                       1
#define OSNS_BACKUP_VALID_NAME                          2
#define OSNS_BACKUP_VALID_MIMETYPE                      4
#define OSNS_BACKUP_VALID_PATH                          8

struct osns_backup_info_s {
    uint32_t                                            hostid;
    uint32_t                                            uniqueid;
    uint32_t                                            valid;
    struct system_timespec_s                            changed;
    struct osns_record_s                                name;
    struct osns_record_s                                mimetypes;
    struct osns_record_s                                path;
};

struct osns_backup_open_s {
    uint32_t                                            hostid;
};

struct osns_backup_open_confirmation_s {
    uint32_t                                            hostid;
};

struct osns_backup_open_failure_s {
    uint32_t                                            hostid;
    uint16_t                                            code;
};

#define OSNS_BACKUP_OPENDIR_FLAG_SINCE                  1
#define OSNS_BACKUP_HANDLE_SIZE                         20

struct osns_backup_opendir_s {
    uint32_t                                            uniqueid;
    uint16_t                                            flags;
    struct system_timespec_s		                since;
};

struct osns_backup_readdir_s {
    char                                                handle[OSNS_BACKUP_HANDLE_SIZE];
    uint64_t                                            offset;
    uint32_t                                            size;
};

#define OSNS_BACKUP_ENTRY_FLAG_DIRECTORY                1
#define OSNS_BACKUP_ENTRY_FLAG_FILE                     2
#define OSNS_BACKUP_ENTRY_FLAG_VERSION                  4

struct osns_backup_entry_s {
    uint32_t                                            uniqueid;
    uint16_t                                            flags;
    struct system_timespec_s                            changed;
    uint16_t                                            len;
    char                                                buffer[];
};

struct osns_backup_closedir_s {
    char                                                handle[OSNS_BACKUP_HANDLE_SIZE];
};

struct osns_backup_stat_s {
    uint32_t                                            uniqueid;
    uint16_t                                            len;
    char                                                path[];
};

struct osns_backup_openfile_s {
    uint32_t                                            unique;
    uint16_t                                            flags;
    uint16_t                                            len;
    char                                                path[];
};

struct osns_backup_readfile_s {
    char                                                handle[OSNS_BACKUP_HANDLE_SIZE];
    uint64_t                                            offset;
    uint32_t                                            size;
};

struct osns_backup_status_s {
    uint32_t                                            status;
};

struct osns_backup_handle_s {
    char                                                handle[OSNS_BACKUP_HANDLE_SIZE];
};

struct osns_backup_data_s {
    uint32_t                                            flags;
    struct osns_record_s                                data;
};

/*
    OSNS_MSG_CONNECTION_ADD
    byte						OSNS_MSG_CONNECTION_ADD
    uint32						id
    record string
	CONNECTION ATTR					attr
    record string
*/

/*
    OSNS_MSG_CONNECTION_SUCCESS
    byte						OSNS_MSG_CONNECTION_SUCCESS
    uint32						id
    uint32						unique
*/

/*
    OSNS_MSG_CONNECTION_FAILURE
    byte						OSNS_MSG_CONNECTION_FAILURE
    uint32						id
    uint32						code
*/

/* REPLIES */

/*
    OSNS_MSG_STATUS
    byte						OSNS_MSG_STATUS
    uint32						id
    uint32						status

*/

/*
    OSNS_MSG_NAME
    byte						OSNS_MSG_NAME
    uint32						id
    byte						length
    bytes[length]					name
*/

/*
    OSNS_MSG_RECORDS
    byte						OSNS_MSG_RECORDS
    uint32						id
    uint32						count
    repeats count times:
	uint16						length
	bytes[length]


	uint32						offset
	uint32						valid
	-- record specific fields --

*/

/*
    OSNS_MSG_EVENT
    byte						OSNS_MSG_EVENT
    uint32						id
    uint8                                               what
    uint32						flags
    uint32 mask                                         mask / valid
    osns record                                         data containing event occurred
                                                        like:
                                                        - PATH
                                                        - NETCACHE ATTR
                                                        - MOUNTINFO
*/

/* OSNS CONTROL INFO
    bytes[2]						type of additional data
    bytes[n]						type specific extra info

    type=OSNS_SOCKET:
    uint32						type of osns system socket
    uint32						flags of osns system socket

*/

#define OSNS_CONTROL_TYPE_OSNS_SOCKET			1

struct osns_control_info_s {
    uint16_t				code;
    union _control_info_u {
	struct osns_socket_type_s {
	    uint32_t			type;
	    uint32_t			flags;
	} osns_socket;
    } info;
};

/*
    OSNS_MSG_MOUNTED
    byte						OSNS_MSG_MOUNTED
    uint32						id
    (optional) bytes[]					control info
*/


struct query_netcache_attr_s {
    uint32_t				flags;
    uint32_t				valid;
    struct name_string_s		names[4];
    uint16_t				port;
    uint16_t				comm_type;
    uint32_t				comm_family;
    uint16_t				service;
    uint16_t				transport;
    struct system_timespec_s		createtime;
    struct system_timespec_s		changetime;
    unsigned int			size;
    unsigned int			pos;
    char				buffer[];
};

struct query_mountinfo_attr_s {
    uint32_t				flags;
    uint32_t				valid;
    struct name_string_s		fs;
    struct name_string_s		source;
    struct osns_record_s		root;
    struct osns_record_s		path;
    uint32_t				major;
    uint32_t				minor;
    struct osns_record_s		options;
    struct system_timespec_s		createtime;
    unsigned int			size;
    unsigned int			pos;
    char				buffer[];
};

struct query_connections_attr_s {
    uint32_t				flags;
    uint32_t				valid;
    struct name_string_s		names[4];
    uint16_t				service;
    uint16_t				transport;
};

struct osns_protocol_s {
    unsigned int			version;
    union _osns_version_u {
	struct osns_version_one_s {
	    unsigned int		sr;
	    unsigned int		sp;
	    unsigned char		handlesize;
	} one;
    } level;
};

struct osns_in_header_s {
    uint32_t				len;
    unsigned char			type;
    uint32_t				id;
};

struct osns_init_s {
    uint32_t				version;
};

struct osns_version_0001_s {
    uint32_t				sr;
    uint32_t				sp;
};

/* queries */

struct osns_openquery_s {
    uint8_t				what;
    uint32_t				flags;
    uint32_t				valid;
};

struct osns_readquery_s {
    uint32_t				size;
    uint32_t				offset;
};

/* mounts */

struct osns_mountcmd_s {
    uint8_t				type;
    uint32_t				maxread;
};

struct osns_umountcmd_s {
    uint8_t				type;
};

struct osns_mounted_s {
    uint32_t				major;
    uint32_t				minor;
};

/* channels */

#define OSNS_CHANNEL_TYPE_NOTSET        0
// #define OSNS_CHANNEL_TYPE_SSH_CHANNEL   1

struct osns_channel_open_s {
    uint32_t                            pid;
    uint8_t				len;
    char				host[];
};

struct osns_channel_open_confirmation_s {
    uint8_t				type;
    uint32_t                            pid;
    uint32_t                            fd;
};

struct osns_ssh_channel_open_data_s {
    uint32_t                            windowsize;
    uint32_t                            maxpacketsize;
};

union osns_channel_open_data_u {
    struct osns_ssh_channel_open_data_s ssh;
};

struct osns_channel_open_failure_s {
    uint32_t				code;
};

struct osns_channel_start_s {
    uint8_t                             type;
    uint32_t                            pid;
    uint32_t                            fd;
};

struct osns_ssh_channel_start_data_s {
    struct ssh_string_s                 request;
    char                                *data;
    unsigned int                        len;
};

union osns_channel_start_data_u {
    struct osns_ssh_channel_start_data_s ssh;
};

/* message over filedescriptor/socket */

struct osns_channel_data_header_s {
    uint8_t                             type;
    uint32_t                            size;
    uint32_t                            code;
};

/* watch */

#define OSNS_WATCH_FLAG_BYID                    1
#define OSNS_WATCH_FLAG_BYDATA                  2
#define OSNS_WATCH_FLAG_RMWATCH_RECURSIVE       4
#define OSNS_WATCH_FLAG_REPLY                   8

struct osns_setwatch_s {
    uint8_t                             what;
    uint32_t				flags;
    uint32_t				properties;
};

/* replies */

struct osns_reply_status_s {
    uint32_t				status;
};

/* prototypes */

#endif

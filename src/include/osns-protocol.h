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

#include "lib/datatypes/ssh-string.h"
#include "lib/datatypes/name-string.h"
#include "lib/system/time.h"
#include "lib/system/path.h"

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

#define OSNS_LIST_TYPE_NETCACHE				1
#define OSNS_LIST_TYPE_MOUNTINFO			2

#define OSNS_MOUNT_TYPE_NETWORK				1
#define OSNS_MOUNT_TYPE_DEVICES				2

#define OSNS_WATCH_TYPE_FSNOTIFY			1

#define OSNS_INIT_FLAG_NETCACHE				(1 << 0)
#define OSNS_INIT_FLAG_MOUNTINFO			(1 << 1)
#define OSNS_INIT_FLAG_MOUNT_NETWORK			(1 << 2)
#define OSNS_INIT_FLAG_MOUNT_DEVICES			(1 << 3)
#define OSNS_INIT_FLAG_FSNOTIFY				(1 << 4)
#define OSNS_INIT_FLAG_DNSLOOKUP			(1 << 5)
#define OSNS_INIT_FLAG_FILTER_NETCACHE			(1 << 6)
#define OSNS_INIT_FLAG_FILTER_MOUNTINFO			(1 << 7)
#define OSNS_INIT_FLAG_SETWATCH_NETCACHE		(1 << 8)

#define OSNS_INIT_FLAG_MOUNT				(OSNS_INIT_FLAG_MOUNT_DEVICES | OSNS_INIT_FLAG_MOUNT_NETWORK)

#define OSNS_INIT_ALL_FLAGS				(OSNS_INIT_FLAG_NETCACHE | OSNS_INIT_FLAG_MOUNTINFO | OSNS_INIT_FLAG_MOUNT_NETWORK | OSNS_INIT_FLAG_MOUNT_DEVICES | OSNS_INIT_FLAG_FSNOTIFY | OSNS_INIT_FLAG_DNSLOOKUP | OSNS_INIT_FLAG_FILTER_NETCACHE | OSNS_INIT_FLAG_FILTER_MOUNTINFO | OSNS_INIT_FLAG_SETWATCH_NETCACHE)

struct osns_system_service_s {
    unsigned int					index;
    unsigned int					initflag;
    char						*name;
};

#define OSNS_DEFAULT_RUNPATH				"/run/osns"
#define OSNS_DEFAULT_UNIXGROUP				"osns"
#define OSNS_DEFAULT_NETWORK_MOUNT_PATH			"/run/network"
#define OSNS_DEFAULT_DEVICES_MOUNT_PATH			"/run/devices"

#define OSNS_DEFAULT_FUSE_MAXREAD			8192

/* OSNS Message Codes */

#define OSNS_MSG_INIT					1
#define OSNS_MSG_VERSION				2
#define OSNS_MSG_DISCONNECT				3
#define OSNS_MSG_UNIMPLEMENTED				4

#define OSNS_MSG_OPENQUERY				10
#define OSNS_MSG_READQUERY				11
#define OSNS_MSG_CLOSEQUERY				12

#define OSNS_MSG_SETWATCH				20
#define OSNS_MSG_RMWATCH				21

#define OSNS_MSG_MOUNTCMD				30
#define OSNS_MSG_UMOUNTCMD				31

#define OSNS_MSG_STATUS					100
#define OSNS_MSG_NAME					101
#define OSNS_MSG_RECORDS				102
#define OSNS_MSG_WATCH					103
#define OSNS_MSG_MOUNTED				104
#define OSNS_MSG_UMOUNTED				105
#define OSNS_MSG_EVENT					106

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

/* SERVICES */

/* NETCACHE */

#define OSNS_NETCACHE_QUERY_FLAG_LOCALHOST		(1 << 0)
#define OSNS_NETCACHE_QUERY_FLAG_PRIVATE		(1 << 1)
#define OSNS_NETCACHE_QUERY_FLAG_DNSSD			(1 << 2)
#define OSNS_NETCACHE_QUERY_FLAG_DNS			(1 << 3)
#define OSNS_NETCACHE_QUERY_FLAG_LOCALNETWORK		(1 << 4)
#define OSNS_NETCACHE_QUERY_FLAG_CREATETIME_AFTER	(1 << 5)
#define OSNS_NETCACHE_QUERY_FLAG_CHANGETIME_AFTER	(1 << 6)
#define OSNS_NETCACHE_QUERY_FLAG_VALID_OR		(1 << 7)
#define OSNS_NETCACHE_QUERY_FLAG_EVENT			(1 << 8)

#define OSNS_NETCACHE_QUERY_INDEX_IPV4			0
#define OSNS_NETCACHE_QUERY_INDEX_IPV6			1
#define OSNS_NETCACHE_QUERY_INDEX_DNSHOSTNAME		2
#define OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN		3

/* OSNS Valid Attributes when requesting/returning a service record */

#define OSNS_NETCACHE_QUERY_ATTR_IPV4			(1 << 0)
#define OSNS_NETCACHE_QUERY_ATTR_IPV6			(1 << 1)
#define OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME		(1 << 2)
#define OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN		(1 << 3)
#define OSNS_NETCACHE_QUERY_ATTR_PORT			(1 << 4)
#define OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY		(1 << 5)
#define OSNS_NETCACHE_QUERY_ATTR_COMM_DOMAIN		OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY
#define OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE		(1 << 6)
#define OSNS_NETCACHE_QUERY_ATTR_SERVICE		(1 << 7)
#define OSNS_NETCACHE_QUERY_ATTR_TRANSPORT		(1 << 8)
#define OSNS_NETCACHE_QUERY_ATTR_CREATETIME		(1 << 9)
#define OSNS_NETCACHE_QUERY_ATTR_CHANGETIME		(1 << 10)

/*
    a service query request/result will have the following format:
    uint32						valid
    name string						ipv4
    name string						ipv6
    name string						dns hostname
    name string						dns domain
    uint16						port				like 22
    uint16						communication type		like SOCK_STREAM, SOCK_DGRAM ...
    uint32						communication family		like AF_INET, AF_INET6, AF_LOCAL ...
    uint16						service				like SFTP, WEBDAV, SMB
    uint16						transport			like SSH
    {int64,int32}					createtime
    {int64,int32}					changetime
*/

#define OSNS_NETCACHE_TRANSPORT_TYPE_SSH		1

#define OSNS_NETCACHE_SERVICE_TYPE_SSH			1
#define OSNS_NETCACHE_SERVICE_TYPE_NFS			2
#define OSNS_NETCACHE_SERVICE_TYPE_SFTP			3
#define OSNS_NETCACHE_SERVICE_TYPE_SMB			4
#define OSNS_NETCACHE_SERVICE_TYPE_WEBDAV		5

/* MOUNTINFO */

/* OSNS Flags for services MOUNTINFO */

#define OSNS_MOUNTINFO_QUERY_FLAG_PSEUDOFS		(1 << 0)
#define OSNS_MOUNTINFO_QUERY_FLAG_VALID_OR		(1 << 1)
#define OSNS_MOUNTINFO_QUERY_FLAG_CREATETIME_AFTER	(1 << 2)

/* OSNS Valid Attributes when requesting/returning a mountinfo record */

#define OSNS_MOUNTINFO_QUERY_ATTR_FS			(1 << 0)
#define OSNS_MOUNTINFO_QUERY_ATTR_SOURCE		(1 << 1)
#define OSNS_MOUNTINFO_QUERY_ATTR_ROOT			(1 << 2)
#define OSNS_MOUNTINFO_QUERY_ATTR_PATH			(1 << 3)
#define OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR	(1 << 4)
#define OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS		(1 << 5)
#define OSNS_MOUNTINFO_QUERY_ATTR_FLAGS			(1 << 6)
#define OSNS_MOUNTINFO_QUERY_ATTR_CREATETIME		(1 << 7)

/*
    a mountinfo query request/result will have the following format:
    uint32						valid
    name string						filesystem
    name string						source
    record string					root
    record string					path
    uint32						devid
    record string					options
    uint32						flags
*/

/*
    a mount command has the following format:
    byte						type (network, devices, ...)
    uint32						maxread
*/


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
    name string						what
    where what is:					"netcache" (via DNSSD)
							"mountinfo"
							...
    uint32						flags
    uint32						valid
    records string
	QUERYATTR						attr to filter the result

    where:
    - flags are various bits which have a meaining within the context of the name what
    - valid is a bit mask of what the client want to reply (==0 -> everything)
    - QUERYATTR are the attibutes to filter on for example a specific transport type (example SSH), a service type (example SFTP) and/or a domain (example.net)

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
*/

/*
    OSNS_MSG_CLOSEQUERY
    byte						OSNS_MSG_CLOSEQUERY
    uint32						id
    name string						handle

    reply with OSNS_MSG_STATUS (ok)
*/

/*
    OSNS_MSG_SETWATCH
    byte						OSNS_MSG_SETWATCH
    uint32						id
    ssh string						what
    what:						"netcache"
							"fsnotify"
							"mountinfo"

    when what=="netcache"
    QUERYATTR						attr to watch

    when what=="fsnotify"
    ssh string						path
    uint32						mask

    when what=="mountinfo"
    MOUNTATTR						attr to watch

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
    OSNS_MSG_WATCH
    byte						OSNS_MSG_WATCH
    uint32						id
*/

/*
    OSNS_MSG_EVENT
    byte						OSNS_MSG_EVENT
    uint32						id
    uint32						count
    repeats count times:
	uint16						length
	bytes[length]
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
    unsigned short			code;
    union _control_info_u {
	struct osns_socket_type_s {
	    unsigned int		type;
	    unsigned int		flags;
	} osns_socket;
    } info;
};

/*
    OSNS_MSG_MOUNTED
    byte						OSNS_MSG_MOUNTED
    uint32						id
    (optional) bytes[]					control info
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

struct osns_protocol_s {
    unsigned int			version;
    union _osns_version_u {
	struct osns_version_one_s {
	    unsigned int		flags;
	    unsigned char		handlesize;
	} one;
    } level;
};

/* prototypes */

#endif

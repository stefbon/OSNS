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
#include "libosns-users.h"
#include "sftp/attr-context.h"
#include "ssh/subsystem/connection.h"

#define SFTP_PROTOCOL_VERSION_DEFAULT			6

struct sftp_subsystem_s;
struct sftp_in_header_s;
typedef void (* sftp_cb_t)(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data);

#define SFTP_RECEIVE_BUFFER_SIZE_DEFAULT		16384
#define SFTP_ERROR_BUFFER_SIZE_DEFAULT		        4096

struct sftp_in_header_s {
    uint32_t                                            len;
    uint8_t                                             type;
    uint32_t                                            id;
};

struct sftp_identity_s {
    struct passwd					pwd;
    struct ssh_string_s 				home;
    unsigned int					size;
    char						*buffer;
};

struct sftp_extensions_s {
    unsigned int					mask;
    unsigned int					mapped;
};

#define SFTP_SUBSYSTEM_FLAG_INIT			(1 << 0)
#define SFTP_SUBSYSTEM_FLAG_VERSION_RECEIVED		(1 << 1)
#define SFTP_SUBSYSTEM_FLAG_VERSION_SEND		(1 << 2)
#define SFTP_SUBSYSTEM_FLAG_SESSION			(1 << 3)

#define SFTP_SUBSYSTEM_FLAG_DISCONNECTING		(1 << 21)
#define SFTP_SUBSYSTEM_FLAG_DISCONNECTED		(1 << 22)
#define SFTP_SUBSYSTEM_FLAG_DISCONNECT			( SFTP_SUBSYSTEM_FLAG_DISCONNECTING | SFTP_SUBSYSTEM_FLAG_DISCONNECTED )

#define SFTP_SUBSYSTEM_FLAG_FINISH			(1 << 27)

struct sftp_protocol_s {
    unsigned char					version;
};

#define SFTP_PREFIX_FLAG_IGNORE_XDEV_SYMLINKS		1
#define SFTP_PREFIX_FLAG_IGNORE_BROKEN_SYMLINKS		2
#define SFTP_PREFIX_FLAG_IGNORE_SPECIAL_FILES		4

struct convert_sftp_path_s {
    void						(* complete)(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct fs_path_s *l);
};

struct sftp_prefix_s {
    unsigned int					flags;
    struct ssh_string_s					path;
    unsigned int					(* get_length_fullpath)(struct sftp_subsystem_s *sftp, struct ssh_string_s *p, struct convert_sftp_path_s *c);
};

#define SFTP_SEND_FLAG_BLOCKED				1
#define SFTP_SEND_FLAG_DISCONNECTING			2
#define SFTP_SEND_FLAG_DISCONNECTED			4
#define SFTP_SEND_FLAG_DISCONNECT			( SFTP_SEND_FLAG_DISCONNECTING | SFTP_SEND_FLAG_DISCONNECTED )

struct sftp_send_s {
    unsigned int					flags;
    /* here array with cb when sending */
};

struct sftp_subsystem_s {
    unsigned int					flags;
    struct sftp_prefix_s				prefix;
    struct sftp_extensions_s				extensions;
    struct net_idmapping_s				mapping;
    struct sftp_protocol_s				protocol;
    struct ssh_subsystem_connection_s			connection;
    struct sftp_identity_s				identity;
    struct shared_signal_s				*signal;
    struct sftp_send_s					send;
    struct attr_context_s				attrctx;
    void                                                (* close)(struct sftp_subsystem_s *sftp, unsigned int level);
    void                                                (* error)(struct sftp_subsystem_s *sftp, unsigned int level, unsigned int errcode);
    sftp_cb_t						cb[256];
};

/* prototypes */

#endif

/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef _SFTP_COMMON_PROTOCOL_H
#define _SFTP_COMMON_PROTOCOL_H

#include "lib/ssh/ssh-string.h"
#include "libosns-list.h"
#include "sftp/protocol.h"

#define SFTP_REQUEST_STATUS_SEND		1
#define SFTP_REQUEST_STATUS_WAITING		2
#define SFTP_REQUEST_STATUS_RESPONSE		4
#define SFTP_REQUEST_STATUS_FINISH		8
#define SFTP_REQUEST_STATUS_INTERRUPT		16
#define SFTP_REQUEST_STATUS_TIMEDOUT		32
#define SFTP_REQUEST_STATUS_DISCONNECT		64
#define SFTP_REQUEST_STATUS_ERROR		128

#define SFTP_EXTENSION_TYPE_DEFAULT						1
#define SFTP_EXTENSION_TYPE_CUSTOM						2

#define SFTP_EXTENSION_EVENT_SUPPORTED						1
#define SFTP_EXTENSION_EVENT_DATA						2
#define SFTP_EXTENSION_EVENT_MAPPED						3
#define SFTP_EXTENSION_EVENT_ERROR						5

/*
    responses from SFTP
    these are almos the same for the different sftp versions
    the differences:

    STATUS
    ------

    - the list of error codes is getting longer for the higher versions
      the body of the message looks like:

	uint32		id
	uint32		status code
	string		error message (ISO-10646 UTF-8)
	string		language tag

    - for version 5 and later error specific data is added:

	error specific data

    DATA
    ----

    - version 6 and later added an optional byte is added:

	bool end-of-list (optional)

    NAME
    ----

    - the buffer in the name response for version 3 looks like:

	uint32		id
	uint32		count
	repeats count times:
	    string filename
	    string longname
	    ATTR   attrs

    - for versions 4-5 it looks like:

	uint32		id
	uint32		count
	repeats count times:
	    string filename [UTF-8]
	    ATTR   attrs

    - for later versions (6..) it looks the same as for versions 4 and 5,
      only and optional byte is added:

	bool end-of-list (optional)

*/

#define SFTP_RESPONSE_FLAG_EOF_SUPPORTED			1
#define SFTP_RESPONSE_FLAG_EOF					2

struct status_response_s {
    unsigned int 		code;
    unsigned int		linux_error;
};

struct data_response_s {
    unsigned char		flags;
};

struct name_response_s {
    unsigned char		flags;
    unsigned int		count;
};

union sftp_response_u {
    struct status_response_s 	status;
    struct data_response_s	data;
    struct name_response_s 	name;
};

struct sftp_reply_s {
    unsigned char		type;
    uint32_t			sequence;
    union sftp_response_u	response;
    char			*data;
    unsigned int		size;
    unsigned int		error;
    void			(* free)(struct sftp_reply_s *reply);
};

/* request to SFTP */

struct sftp_init_s {
    unsigned int		version;
};

struct sftp_path_s {
    unsigned char		*path;
    unsigned int 		len;
};

struct sftp_handle_s {
    unsigned int		len;
    unsigned char		*handle;
};

struct sftp_open_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned int		posix_flags;
    unsigned int		size;
    unsigned char		*buff;
};

struct sftp_read_s {
    unsigned int		len;
    unsigned char		*handle;
    uint64_t			offset;
    uint64_t			size;
};

struct sftp_write_s {
    unsigned int		len;
    unsigned char		*handle;
    uint64_t			offset;
    uint64_t			size;
    char			*data;
};

struct sftp_rename_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned char		*target_path;
    unsigned int		target_len;
    unsigned int		posix_flags;
};

struct sftp_mkdir_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned int		size;
    unsigned char		*buff;
};

struct sftp_getstat_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned int		valid;
};

struct sftp_setstat_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned int		size;
    unsigned char		*buff;
};

struct sftp_fgetstat_s {
    unsigned char		*handle;
    unsigned int 		len;
    unsigned int		valid;
};

struct sftp_fsetstat_s {
    unsigned char		*handle;
    unsigned int 		len;
    unsigned int		size;
    unsigned char		*buff;
};

struct sftp_link_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned char		*target_path;
    unsigned int		target_len;
    unsigned char		symlink;
};

struct sftp_symlink_s {
    unsigned char		*path;
    unsigned int		len;
    unsigned char		*target_path;
    unsigned int		target_len;
};

struct sftp_block_s {
    unsigned char		*handle;
    unsigned int 		len;
    uint64_t			offset;
    uint64_t			size;
    uint32_t			type;
};

struct sftp_unblock_s {
    unsigned char		*handle;
    unsigned int 		len;
    uint64_t			offset;
    uint64_t			size;
};

struct sftp_data_s {
    unsigned char		*data;
    unsigned int		len;
};

struct sftp_mapextension_s {
    unsigned int		len;
    char			*name;
    unsigned char		domap;
};

struct sftp_fstatat_s {
    struct sftp_handle_s	handle;
    struct sftp_path_s		path;
    uint32_t			valid;
    uint32_t			property;
    uint32_t			flags;
};

struct sftp_extension_s {
    unsigned char		type;
    void			*ptr;
    union {
	struct _extension_name_s {
	    unsigned char		*name;
	    unsigned int		len;
	} name;
	unsigned char			nr;
    } id;
    unsigned int		size;
    unsigned char		*data;
};

struct sftp_request_s {
    unsigned int			status;
    unsigned int			id;
    unsigned int			unique;
    struct context_interface_s		*interface;
    void				*ptr;
    struct list_element_s		list;
    struct list_element_s		slist;
    struct system_timespec_s		started;
    struct system_timespec_s		timeout;
    union {
	struct sftp_init_s		init;
	struct sftp_getstat_s		stat;
	struct sftp_getstat_s		lstat;
	struct sftp_setstat_s		setstat;
	struct sftp_fgetstat_s		fgetstat;
	struct sftp_fsetstat_s		fsetstat;
	struct sftp_open_s		open;
	struct sftp_path_s		opendir;
	struct sftp_read_s		read;
	struct sftp_write_s		write;
	struct sftp_handle_s		fsync;
	struct sftp_handle_s		readdir;
	struct sftp_handle_s		close;
	struct sftp_path_s		remove;
	struct sftp_rename_s		rename;
	struct sftp_mkdir_s		mkdir;
	struct sftp_path_s		rmdir;
	struct sftp_path_s		readlink;
	struct sftp_link_s 		link;
	struct sftp_symlink_s 		symlink;
	struct sftp_block_s		block;
	struct sftp_unblock_s		unblock;
	struct sftp_handle_s		fstatvfs;
	struct sftp_path_s		statvfs;
	struct sftp_path_s		realpath;
	struct sftp_mapextension_s	mapext;
	struct sftp_fstatat_s		fstatat;
	struct sftp_extension_s 	extension;
    } call;
    struct sftp_reply_s			reply;
    int					(* send)(struct sftp_request_s *r, char *data, unsigned int size, uint32_t *seq, struct list_element_s *l);
};

#endif

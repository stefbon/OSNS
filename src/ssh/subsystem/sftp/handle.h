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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_HANDLE_H
#define OSNS_SSH_SUBSYSTEM_SFTP_HANDLE_H

#include "ssh/subsystem/commonhandle.h"

/*
    every handle has the form:
    dev || inode || pid || fd || type

    - 4 bytes				dev
    - 8 bytes				inode
    - 4 bytes				pid
    - 4 bytes				fd
    - 1 byte				type

    ---------
    21 bytes

    - encrypt and decrypt?

*/

#define SFTP_HANDLE_SIZE					21

#define FILEHANDLE_BLOCK_READ					1
#define FILEHANDLE_BLOCK_WRITE					2
#define FILEHANDLE_BLOCK_DELETE					4

union serverflags_u {
    struct sftp_s {
	unsigned int						access;
	unsigned int						flags;
    } sftp;
};

struct sftp_filehandle_s {
    struct commonhandle_s					handle;
    struct sftp_subsystem_s					*sftp;
    // unsigned int						posix_flags;
    // union serverflags_u						server;
    // int								(* get_lock_flags)(struct filehandle_s *h, const char *what);
};

struct insert_filehandle_s {
    unsigned int						status;
    unsigned int						error;
    union serverflags_u						server;
    union {
	struct lockconflict_s {
	    uid_t						uid;
	    struct ip_address_s					address;
	} lock;
    } info;
};

struct sftp_dirhandle_s {
    struct commonhandle_s					handle;
    struct sftp_subsystem_s					*sftp;
    unsigned int						size;
    char							*buffer;
    unsigned int						pos;
    unsigned int						read;
    unsigned int						flags;
    unsigned int						valid;
    void 							(* readdir)(struct sftp_dirhandle_s *d, struct sftp_payload_s *p);
};


/* prototypes */

unsigned char write_sftp_filehandle(struct sftp_filehandle_s *filehandle, char *buffer);
struct sftp_filehandle_s *insert_sftp_filehandle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino, unsigned int fd);
struct sftp_filehandle_s *find_sftp_filehandle_buffer(struct sftp_subsystem_s *sftp, char *buffer);

int release_sftp_handle_buffer(char *buffer, struct sftp_subsystem_s *sftp);

unsigned char write_sftp_dirhandle(struct sftp_dirhandle_s *dirhandle, char *buffer);
struct sftp_dirhandle_s *insert_sftp_dirhandle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino, unsigned int fd);
struct sftp_dirhandle_s *find_sftp_dirhandle_buffer(struct sftp_subsystem_s *s, char *buffer);

#endif

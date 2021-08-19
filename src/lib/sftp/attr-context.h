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

#ifndef LIB_SFTP_ATTR_CONTEXT_H
#define LIB_SFTP_ATTR_CONTEXT_H

#include "attr/buffer.h"

#define WRITE_ATTR_VB_SIZE				0

#define WRITE_ATTR_VB_UIDGID				1
#define WRITE_ATTR_VB_OWNERGROUP			1

#define WRITE_ATTR_VB_PERMISSIONS			2

#define WRITE_ATTR_VB_ACMODTIME				3
#define WRITE_ATTR_VB_ATIME				3
#define WRITE_ATTR_VB_MTIME				4
#define WRITE_ATTR_VB_BTIME				5
#define WRITE_ATTR_VB_CTIME				6

#define WRITE_ATTR_VB_MAX				6

#define WRITE_ATTR_NT_ATIME				0
#define WRITE_ATTR_NT_MTIME				1
#define WRITE_ATTR_NT_BTIME				2
#define WRITE_ATTR_NT_CTIME				3

#define WRITE_ATTR_NT_MAX				3

#define VALIDFLAGS_ATTR_MAX_COUNT				( WRITE_ATTR_VB_MAX + 1 )
#define VALIDFLAGS_NTIME_MAX_COUNT				( WRITE_ATTR_NT_MAX + 1 )

struct rw_attr_result_s {
    unsigned int					valid;
    unsigned int					todo;
    unsigned int					done;
    struct _rw_attrcb_s					*attrcb;
    unsigned int					count;
    unsigned char					vb[VALIDFLAGS_ATTR_MAX_COUNT];
    struct _rw_attrcb_s					*ntimecb;
    unsigned char					nt[VALIDFLAGS_NTIME_MAX_COUNT];
};

struct attr_context_s;
typedef void (* rw_attr_cb)(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr);

struct _rw_attrcb_s {
    uint32_t						code;
    unsigned char					shift;
    rw_attr_cb						cb[2];
};

struct sftp_user_s;
struct sftp_group_s;

struct attr_context_s {
    void		*ptr;
    void		(* get_local_uid_byid)(struct attr_context_s *ctx, struct sftp_user_s *u);
    void		(* get_local_gid_byid)(struct attr_context_s *ctx, struct sftp_group_s *g);
    void		(* get_local_uid_byname)(struct attr_context_s *ctx, struct sftp_user_s *u);
    void		(* get_local_gid_byname)(struct attr_context_s *ctx, struct sftp_group_s *g);
    void		(* get_remote_uid_byid)(struct attr_context_s *ctx, struct sftp_user_s *u);
    void		(* get_remote_gid_byid)(struct attr_context_s *ctx, struct sftp_group_s *g);
    void		(* get_remote_username_byid)(struct attr_context_s *ctx, struct sftp_user_s *u);
    void		(* get_remote_groupname_byid)(struct attr_context_s *ctx, struct sftp_group_s *g);
    unsigned int	(* maxlength_username)(struct attr_context_s *ctx);
    unsigned int	(* maxlength_groupname)(struct attr_context_s *ctx);
    unsigned int	(* maxlength_domainname)(struct attr_context_s *ctx);
    unsigned char 	(* get_sftp_protocol_version)(struct attr_context_s *ctx);
    unsigned int	(* get_sftp_flags)(struct attr_context_s *ctx, const char *what);
};

/* prototypes */

void init_attr_context(struct attr_context_s *ctx, void *ptr);

#endif

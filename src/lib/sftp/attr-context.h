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

#include "system.h"
#include "attr/buffer.h"
#include "lib/users/mapping.h"

#define SSH_FILEXFER_INDEX_NSEC_ATIME			32
#define SSH_FILEXFER_INDEX_NSEC_MTIME			33
#define SSH_FILEXFER_INDEX_NSEC_BTIME			34
#define SSH_FILEXFER_INDEX_NSEC_CTIME			35

#define RW_ATTR_RESULT_FLAG_READ			1
#define RW_ATTR_RESULT_FLAG_WRITE			2
#define RW_ATTR_RESULT_FLAG_CACHED			4

struct attr_context_s;
struct rw_attr_result_s;

typedef void (* rw_attr_cb)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);

struct rw_attr_result_s {
    unsigned int					flags;
    unsigned int					valid;
    unsigned int					todo;
    unsigned int					done;
    unsigned int					ignored;
    unsigned int					stat_mask;
    void 						(* parse_attribute)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr);
    void						*ptr;
};

void parse_dummy(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr);


#define RW_ATTR_RESULT_INIT				{0, 0, 0, 0, 0, 0, parse_dummy, NULL}

struct _rw_attrcb_s {
    uint32_t						code;
    unsigned char					shift;
    unsigned int					stat_mask;
    rw_attr_cb						r_cb;
    rw_attr_cb						w_cb;
    unsigned int					maxlength;
    const char						*name;
};

struct attr_ops_s {
    void						(* parse_attributes)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *result, struct system_stat_s *stat);
    void						(* read_name_name_response)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name);
    void						(* read_attr_name_response)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *result, struct system_stat_s *stat);
    void						(* write_name_name_response)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name);
    void						(* write_attr_name_response)(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *result, struct system_stat_s *stat);
};

#define ATTR_CONTEXT_FLAG_CLIENT			1
#define ATTR_CONTEXT_FLAG_SERVER			2

struct attr_context_s {
    unsigned int					flags;
    void						*ptr;
    struct net_idmapping_s 				*mapping;
    struct attr_ops_s					ops;
    unsigned int					w_valid;
    unsigned int					w_count;
    unsigned int					r_valid;
    unsigned int					r_count;
    struct _rw_attrcb_s					attrcb[36];
    unsigned int					(* maxlength_filename)(struct attr_context_s *actx);
    unsigned int					(* maxlength_username)(struct attr_context_s *actx);
    unsigned int					(* maxlength_groupname)(struct attr_context_s *actx);
    unsigned int					(* maxlength_domainname)(struct attr_context_s *actx);
    unsigned char 					(* get_sftp_protocol_version)(struct attr_context_s *actx);
    unsigned int					(* get_sftp_flags)(struct attr_context_s *actx, const char *what);
};

/* prototypes */

void init_attrcb_zero(struct _rw_attrcb_s *attrcb, unsigned int count);
void init_attr_context(struct attr_context_s *actx, unsigned int flags, void *ptr, struct net_idmapping_s *m);
void set_sftp_attr_context(struct attr_context_s *actx);

unsigned int get_supported_valid_flags(struct attr_context_s *actx, unsigned char what);

#endif

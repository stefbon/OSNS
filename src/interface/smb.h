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

#ifndef INTERFACE_SMB_H
#define INTERFACE_SMB_H

#include "smb-signal.h"

struct smb_share_s;

struct smb_context_s {
    uint64_t						unique;
    void 						*interface;
    int							(* signal_ctx2share)(struct smb_share_s *s, const char *what, struct ctx_option_s *o);
    int							(* signal_share2ctx)(struct smb_share_s *s, const char *what, struct ctx_option_s *o);
};

#define _SMB_SHARE_FLAG_INIT				1
#define _SMB_SHARE_FLAG_CONNECTED			2
#define _SMB_SHARE_FLAG_STARTED				4
#define _SMB_SHARE_FLAG_CLOSED				8
#define _SMB_SHARE_FLAG_FREE				16
#define _SMB_SHARE_FLAG_ERROR				32

struct smb_share_s {
    unsigned int					flags;
    unsigned int					error;
    uint32_t						id;
    struct smb_context_s				context;
    struct list_header_s				requests;
    struct smb_signal_s					signal;
    struct bevent_s 					*bevent;
    void						*ptr;
};

/* prototypes */

void init_smb_share_interface();
void get_smb_request_timeout_ctx(struct context_interface_s *interface, struct timespec *timeout);
struct smb_signal_s *get_smb_signal_ctx(struct context_interface_s *interface);

void add_smb_list_pending_requests_ctx(struct context_interface_s *interface, struct list_element_s *list);
void remove_smb_list_pending_requests_ctx(struct context_interface_s *interface, struct list_element_s *list);

#define SMB_SHARE_TYPE_DISKTREE						1
#define SMB_SHARE_TYPE_PRINTQ						2
#define SMB_SHARE_TYPE_DEVICE						3
#define SMB_SHARE_TYPE_IPC						4

#define SMB_SHARE_FLAG_TEMPORARY					1
#define SMB_SHARE_FLAG_HIDDEN						2
#define SMB_SHARE_FLAG_ERROR						4

int smb_share_enum_async_ctx(struct context_interface_s *interface, void (* cb)(struct context_interface_s *interface, char *name, unsigned int type, unsigned int flags, void *ptr), void *ptr);

uint32_t get_id_smb_share(struct context_interface_s *interface);

void fill_inode_attr_smb(struct context_interface_s *interface, struct stat *st, void *ptr);

#endif

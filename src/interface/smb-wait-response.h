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

#ifndef INTERFACE_SMB_WAIT_RESPONSE_H
#define INTERFACE_SMB_WAIT_RESPONSE_H

struct smb_data_s {
    struct context_interface_s 			*interface;
    struct list_element_s			list;
    uint32_t					id;
    void					*ptr;
    unsigned int				size;
    char					buffer[];
};

#define SMB_REQUEST_STATUS_WAITING		1
#define SMB_REQUEST_STATUS_RESPONSE		2
#define SMB_REQUEST_STATUS_FINISH		6
#define SMB_REQUEST_STATUS_INTERRUPT		8
#define SMB_REQUEST_STATUS_TIMEDOUT		16
#define SMB_REQUEST_STATUS_DISCONNECT		32
#define SMB_REQUEST_STATUS_ERROR		64

struct smb_request_s {
    unsigned int				status;
    unsigned int				error;
    uint32_t					id;
    struct context_interface_s 			*interface;
    void					*ptr;
    struct list_element_s			list;
    struct system_timespec_s			started;
    struct system_timespec_s			timeout;
    struct smb_data_s				*data;
};

/* prototypes */

uint32_t get_smb_unique_id(struct context_interface_s *interface);
struct smb_request_s *get_smb_request(struct context_interface_s *interface, unsigned int id, struct generic_error_s *error);
int signal_smb_received_id(struct context_interface_s *interface, struct smb_request_s *r);
void signal_smb_received_id_error(struct context_interface_s *interface, struct smb_request_s *r, struct generic_error_s *error);

void init_smb_request(struct smb_request_s *r, struct context_interface_s *i, struct fuse_request_s *f_request);
void init_smb_request_minimal(struct smb_request_s *r, struct context_interface_s *i);
unsigned char wait_smb_response_ctx(struct context_interface_s *i, struct smb_request_s *r, struct system_timespec_s *timeout);

#endif

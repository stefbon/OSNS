/*
  2018 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_SSH_PAYLOAD_H
#define LIB_SSH_PAYLOAD_H

#include "libosns-list.h"
#include "datatypes/ssh-msg-buffer.h"

struct ssh_payload_s {
    unsigned char			                type;
    uint32_t				                sequence;
    uint32_t                                            code;
    struct list_element_s                               list;
    unsigned int			                size;
    unsigned int                                        len;
    char				                buffer[];
};

struct payload_queue_s {
    struct list_header_s 				header;
    struct shared_signal_s				*signal;
    void						*ptr;
};

/* prototypes */

void set_msg_buffer_payload(struct msg_buffer_s *mb, struct ssh_payload_s *p);

struct ssh_payload_s *realloc_payload_static(struct ssh_payload_s *payload, unsigned int size);
struct ssh_payload_s *realloc_payload_dynamic(struct ssh_payload_s *payload, unsigned int size);

char *isolate_payload_buffer_dynamic(struct ssh_payload_s **p_payload, unsigned int pos, unsigned int size);
char *isolate_payload_buffer_static(struct ssh_payload_s **p_payload, unsigned int pos, unsigned int size);

void free_payload(struct ssh_payload_s **p);
void init_ssh_payload(struct ssh_payload_s *p, unsigned int size);

struct ssh_payload_s *get_ssh_payload(struct payload_queue_s *queue, struct system_timespec_s *expire,
                    int (* cb_select)(struct ssh_payload_s *payload, void *ptr), unsigned char (* cb_break)(void *ptr), void (* cb_error)(unsigned int errcode, void *ptr), void *ptr);

void queue_ssh_payload(struct payload_queue_s *queue, struct ssh_payload_s *payload);
void queue_ssh_broadcast(struct payload_queue_s *queue);

void init_payload_queue(struct shared_signal_s *signal, struct payload_queue_s *queue);
void clear_payload_queue(struct payload_queue_s *queue, unsigned char dolog);

#endif

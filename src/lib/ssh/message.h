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

#ifndef LIB_SSH_MESSAGE_H
#define LIB_SSH_MESSAGE_H

#include "ssh-string.h"

/* prototypes */

struct ssh_channel_open_data_s {
    unsigned char type;
    union _ssh_channel_open_type_u {
        struct _ssh_channel_open_socket_tcpip_s {
            struct ssh_string_s                             address;
            unsigned int                                    portnr;
            struct ssh_string_s                             orig_ip;
            unsigned int                                    orig_portnr;
        } tcpip;
        struct _ssh_channel_open_socket_local_s {
            struct ssh_string_s                             path;
        } local;
        struct _ssh_channel_custom_s {
            char                                            *ptr;
            unsigned int                                    len;
        } custom;
    } data;
};

struct ssh_channel_open_msg_s {
    unsigned int                                rcnr;
    unsigned int                                windowsize;
    unsigned int                                maxpacketsize;
    struct ssh_channel_open_data_s             *data;
};

struct ssh_channel_open_failure_msg_s {
    unsigned int                                reason;
    struct ssh_string_s                         description;
    struct ssh_string_s                         language;
};

struct ssh_channel_close_msg_s {
    unsigned char                               type;
};

struct ssh_channel_windowadjust_msg_s {
    unsigned int                                increase;
};

struct ssh_channel_data_msg_s {
    unsigned int                                len;
    char                                        *data;
};

struct ssh_channel_xdata_msg_s {
    unsigned int                                code;
    unsigned int                                len;
    char                                        *data;
};

struct ssh_channel_request_msg_s {
    struct ssh_string_s                         type;
    unsigned char                               reply;
    unsigned int                                len;
    char                                        *data;
};

struct ssh_channel_msg_s {
    unsigned int                                        lcnr;
    unsigned int                                        sequence;
    union ssh_channel_message_u {
        struct ssh_channel_open_msg_s                   open;
        struct ssh_channel_open_failure_msg_s           open_failure;
        struct ssh_channel_close_msg_s                  close;
        struct ssh_channel_windowadjust_msg_s           windowadjust;
        struct ssh_channel_data_msg_s                   data;
        struct ssh_channel_xdata_msg_s                  xdata;
        struct ssh_channel_request_msg_s                request;
    } type;
};

union ssh_message_u {
    struct ssh_channel_msg_s                            channel;
};

#endif

/*
  2010, 2011, 2012, 2013 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_EVENTLOOP_BEVENT_WRITE_H
#define _LIB_EVENTLOOP_BEVENT_WRITE_H

#include "libosns-list.h"
#include "libosns-error.h"

#include "beventloop.h"

#define WRITE_BEVENT_FLAG_SUCCESS	1
#define WRITE_BEVENT_FLAG_PARTIAL	2
#define WRITE_BEVENT_FLAG_ERROR		4
#define WRITE_BEVENT_FLAG_CLOSE		8

struct bevent_write_data_s {
    unsigned int			flags;
    char				*data;
    unsigned int			size;
    unsigned int			byteswritten;
    struct system_timespec_s		timeout;
    void				*ptr;
    struct generic_error_s		error;
};

/* Prototypes */

int write_socket_signalled(struct bevent_s *bevent, struct bevent_write_data_s *bdata, int (* write_cb)(struct osns_socket_s *sock, char *data, unsigned int size, void *ptr));
void enable_bevent_write_watch(struct bevent_s *bevent);

#endif

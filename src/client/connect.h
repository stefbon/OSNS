/*
  2010, 2011, 2012 Stef Bon <stefbon@gmail.com>

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

#ifndef CLIENT_CONNECT_H
#define CLIENT_CONNECT_H

/* prototypes */

unsigned int connect_context_service(struct service_context_s *ctx, unsigned int service, void (* cb_success)(struct service_context_s *ctx, uint64_t dbid, void *ptr), void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr), void *ptr);
void start_thread_connect_browse_service(struct service_context_s *ctx);

#endif

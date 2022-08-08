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

#ifndef OSNS_REPLY_H
#define OSNS_REPLY_H

/* prototypes */

int osns_reply_init(struct osns_receive_s *r, unsigned int version, unsigned int services);
int osns_reply_status(struct osns_receive_s *r, uint32_t id, unsigned int status, char *extra, unsigned int len);
int osns_reply_name(struct osns_receive_s *r, uint32_t id, struct name_string_s *name);
int osns_reply_records(struct osns_receive_s *r, uint32_t id, unsigned int count, char *records, unsigned int len);

int osns_reply_mounted(struct osns_receive_s *r, uint32_t id, struct system_socket_s *tosend);
int osns_reply_umounted(struct osns_receive_s *r, uint32_t id);

#endif

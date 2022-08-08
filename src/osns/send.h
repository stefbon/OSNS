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

#ifndef OSNS_SEND_H
#define OSNS_SEND_H

/* prototypes */

int send_osns_msg_init(struct osns_receive_s *r, unsigned int version, unsigned int flags);

int send_osns_msg_openquery(struct osns_receive_s *r, uint32_t id, struct name_string_s *command, unsigned int flags, unsigned int valid, struct osns_record_s *attr);
int send_osns_msg_readquery(struct osns_receive_s *r, uint32_t id, struct name_string_s *handle, unsigned int size, unsigned int offset);
int send_osns_msg_closequery(struct osns_receive_s *r, uint32_t id, struct name_string_s *handle);

int send_osns_msg_mountcmd(struct osns_receive_s *r, uint32_t id, unsigned char type, unsigned int maxread);
int send_osns_msg_umountcmd(struct osns_receive_s *r, uint32_t id, unsigned char type);

int send_osns_msg_setwatch(struct osns_receive_s *r, uint32_t id, struct name_string_s *command);
int send_osns_msg_rmwatch(struct osns_receive_s *r, uint32_t id, uint32_t watchid);

#endif

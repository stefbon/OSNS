/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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

#ifndef _SSH_CHANNEL_INIT_H
#define _SSH_CHANNEL_INIT_H

/* prototypes */

void clean_ssh_channel_queue(struct ssh_channel_s *channel);
void clear_ssh_channel(struct ssh_channel_s *channel);
void free_ssh_channel(struct ssh_channel_s **channel);

void init_ssh_channel(struct ssh_session_s *session, struct ssh_connection_s *connection, struct ssh_channel_s *channel, unsigned char type);
struct ssh_channel_s *create_channel(struct ssh_session_s *session, struct ssh_connection_s *c, unsigned char type);
struct ssh_channel_s *open_new_channel(struct ssh_connection_s *connection, struct ssh_string_s *type, unsigned int remote_channel, unsigned int windowsize, unsigned int maxpacketsize, struct ssh_string_s *data);

unsigned int get_ssh_channel_buffer_size();

#endif

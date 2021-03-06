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

#ifndef _SSH_SEND_MSG_CHANNEL_H
#define _SSH_SEND_MSG_CHANNEL_H

/* prototypes */

int send_channel_open_message(struct ssh_channel_s *channel, uint32_t *seq);
int send_channel_close_message(struct ssh_channel_s *channel);
int send_channel_window_adjust_message(struct ssh_channel_s *channel, unsigned int increase);
int send_channel_start_command_message(struct ssh_channel_s *channel, unsigned char reply, uint32_t *seq);
int send_channel_data_message_connected(struct ssh_channel_s *channel, char *data, unsigned int size, uint32_t *seq);
int send_channel_data_message_error(struct ssh_channel_s *channel, char *data, unsigned int size, uint32_t *seq);
int send_channel_data_message(struct ssh_channel_s *channel, char *data, unsigned int len, uint32_t *seq);
int send_channel_request_message(struct ssh_channel_s *channel, const char *request, unsigned char reply, struct ssh_string_s *data, uint32_t *seq);

int send_channel_open_confirmation(struct ssh_channel_s *channel, struct ssh_string_s *data, uint32_t *seq);
int send_channel_open_failure(struct ssh_connection_s *c, unsigned int remote_channel, uint32_t code, uint32_t *seq);

#endif

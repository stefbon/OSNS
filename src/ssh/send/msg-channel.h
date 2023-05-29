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

int send_ssh_channel_open_message(struct ssh_connection_s *c, unsigned char type, union ssh_message_u *msg);

unsigned int get_length_ssh_channel_open_confirmation_msg(struct ssh_channel_open_msg_s *msg);
unsigned int write_ssh_channel_open_comfirmation_msg(unsigned int target_channel, struct ssh_channel_open_msg_s *msg, char *buffer);
int send_ssh_channel_open_confirmation_msg(struct ssh_channel_s *channel, union ssh_message_u *msg);

unsigned int get_length_ssh_channel_open_failure_msg(struct ssh_channel_open_failure_msg_s *msg);
unsigned int write_ssh_channel_open_failure_msg(unsigned int target_channel, struct ssh_channel_open_failure_msg_s *msg, char *buffer);
int send_ssh_channel_open_failure_msg(struct ssh_connection_s *c, unsigned int remote_channel, union ssh_message_u *msg);

unsigned int get_length_ssh_channel_close_msg(struct ssh_channel_close_msg_s *close);
unsigned int write_ssh_channel_close_msg(unsigned int target_channel, struct ssh_channel_close_msg_s *close, char *buffer);
int send_ssh_channel_eofclose_msg(struct ssh_channel_s *channel, union ssh_message_u *msg);

unsigned int get_length_ssh_channel_windowadjust_msg(struct ssh_channel_windowadjust_msg_s *adjust);
unsigned int write_ssh_channel_windowadjust_msg(unsigned int target_channel, struct ssh_channel_windowadjust_msg_s *adjust, char *buffer);
int send_ssh_channel_window_adjust_msg(struct ssh_channel_s *channel, union ssh_message_u *msg);

unsigned int get_length_ssh_channel_request_msg(struct ssh_channel_request_msg_s *msg);
unsigned int write_ssh_channel_request_msg(unsigned int target_channel, struct ssh_channel_request_msg_s *msg, char *buffer);
int send_ssh_channel_request_msg(struct ssh_channel_s *channel, union ssh_message_u *msg);

int send_ssh_channel_start_command_msg(struct ssh_channel_s *channel, unsigned char reply);

unsigned int get_length_ssh_channel_data_msg(struct ssh_channel_data_msg_s *msg);
unsigned int write_ssh_channel_data_msg(unsigned int target_channel, struct ssh_channel_data_msg_s *msg, char *buffer);
int send_ssh_channel_data_msg(struct ssh_channel_s *channel, union ssh_message_u *msg);

unsigned int get_length_ssh_channel_xdata_msg(struct ssh_channel_xdata_msg_s *msg);
unsigned int write_ssh_channel_xdata_msg(unsigned int target_channel, struct ssh_channel_xdata_msg_s *msg, char *buffer);
int send_ssh_channel_xdata_msg(struct ssh_channel_s *channel, union ssh_message_u *msg);

#endif

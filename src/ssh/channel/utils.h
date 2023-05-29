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

#ifndef _SSH_CHANNEL_UTILS_H
#define _SSH_CHANNEL_UTILS_H

#define SSH_CHANNEL_START_COMMAND_FLAG_REPLY				1
#define SSH_CHANNEL_START_COMMAND_FLAG_DATA				2
#define SSH_CHANNEL_START_COMMAND_FLAG_ERROR				4
#define SSH_CHANNEL_START_COMMAND_FLAG_UNEXPECTED			8
#define SSH_CHANNEL_START_COMMAND_FLAG_ALL				16

/* prototypes */

const char *get_ssh_channel_open_failure_reason(unsigned int reason);

void get_ssh_channel_expire_custom(struct ssh_channel_s *channel, struct system_timespec_s *expire);
void get_ssh_channel_expire_init(struct ssh_channel_s *channel, struct system_timespec_s *expire);

unsigned int get_ssh_channel_interface_info(struct ssh_channel_s *channel, char *buffer, unsigned int size);

unsigned int get_ssh_channel_exit_signal(struct ssh_string_s *name);

struct ssh_channel_s *create_ssh_session_channel(struct ssh_session_s *session, const char *what, char *command);
int ssh_channel_start_command(struct ssh_channel_s *channel, unsigned int flags, void (* cb)(struct ssh_channel_s *c, struct ssh_payload_s **p, unsigned int flags, unsigned int errcode, void *ptr), void *ptr);

#endif

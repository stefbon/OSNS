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

/* prototypes */

const char *get_openfailure_reason(unsigned int reason);
void get_channel_expire_init(struct ssh_channel_s *channel, struct timespec *expire);

void get_timeinfo_ssh_server(struct ssh_session_s *session);
unsigned int get_channel_interface_info(struct ssh_channel_s *channel, char *buffer, unsigned int size);

void switch_msg_channel_receive_data(struct ssh_channel_s *channel, const char *name, void (* cb)(struct ssh_channel_s *c, char **b, unsigned int size, uint32_t seq, unsigned char f));
void switch_channel_send_data(struct ssh_channel_s *channel, const char *what);

#endif

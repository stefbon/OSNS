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

#ifndef _SSH_CHANNEL_TABLE_H
#define _SSH_CHANNEL_TABLE_H

/* prototypes */

int lookup_ssh_channel_for_iocb(unsigned int lcnr, struct ssh_payload_s **p_payload, unsigned char iocbnr);
int lookup_ssh_channel_for_cb(unsigned int lcnr, struct ssh_payload_s **p_payload, int (* cb)(struct ssh_channel_s *c, struct ssh_payload_s **p, void *ptr), void *ptr);

void init_ssh_channels_table(struct shared_signal_s *signal);
void clear_ssh_channels_table();

void ssh_channel_table_writelock(unsigned int row);
void ssh_channel_table_writeunlock(unsigned int row);
struct ssh_channel_s *get_next_ssh_channel(struct ssh_channel_s *channel, unsigned int row);

int add_ssh_channel(struct ssh_channel_s *channel, unsigned int flags);
void remove_ssh_channel(struct ssh_channel_s *channel, unsigned int flags, unsigned char locked);

unsigned int get_ssh_channels_tablesize();
struct ssh_channel_s *walk_ssh_channels(int (* cb)(struct ssh_channel_s *c, void *ptr), void *ptr);

#endif

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

#ifndef OSNS_CTL_HASHTABLE_H
#define OSNS_CTL_HASHTABLE_H

/* prototypes */

void init_osns_client_hashtable();

uint32_t get_osns_msg_id(struct osns_receive_s *r);
void hash_osns_packet(struct osns_receive_s *r, struct osns_packet_s *packet);
void unhash_osns_packet(struct osns_receive_s *r, struct osns_packet_s *packet);
void process_osns_reply(struct osns_receive_s *r, unsigned char type, uint32_t id, char *data, unsigned int len, struct osns_control_s *ctrl);
void wait_osns_packet(struct osns_receive_s *r, struct osns_packet_s *packet);

void remove_permanent_packet(uint32_t id);

#endif

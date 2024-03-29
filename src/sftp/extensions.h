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

#ifndef _SFTP_EXTENSIONS_H
#define _SFTP_EXTENSIONS_H

/* prototypes */

void init_sftp_extensions(struct sftp_client_s *sftp);
void clear_sftp_extensions(struct sftp_client_s *sftp);
int complete_sftp_extensions(struct sftp_client_s *sftp);

int add_protocol_extension_server(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data);
int send_sftp_extension_index(struct sftp_client_s *sftp, unsigned int index, struct sftp_request_s *sftp_r);
unsigned int get_sftp_protocol_extension_index(struct sftp_client_s *sftp, struct ssh_string_s *name);
unsigned int get_sftp_protocol_extension_count(struct sftp_client_s *sftp);

struct ssh_string_s *get_supported_next_extension_name(struct sftp_client_s *sftp, struct list_element_s **p_list, unsigned char who);

#endif

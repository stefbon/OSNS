/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_EXTENSIONS_H
#define OSNS_SSH_SUBSYSTEM_SFTP_EXTENSIONS_H

#define SFTP_PROTOCOL_EXTENSION_FLAG_MAPPED		1

struct sftp_protocol_extension_s {
    unsigned int					flags;
    const char						*name;
    unsigned char					code;
    void						(* cb)(struct sftp_payload_s *p, unsigned int pos);
    void						(* op)(struct sftp_payload_s *p);
};

/* prototypes */

struct sftp_protocol_extension_s *get_next_sftp_protocol_extension(struct sftp_protocol_extension_s *ext, unsigned int mask);
struct sftp_protocol_extension_s *find_sftp_protocol_extension(struct ssh_string_s *name, unsigned int mask);

void sftp_op_extension(struct sftp_payload_s *payload);

#endif

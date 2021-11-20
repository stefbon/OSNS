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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_INIT_H
#define OSNS_SSH_SUBSYSTEM_SFTP_INIT_H

#include "osns_sftp_subsystem.h"

struct sftp_init_extensions_s {
    unsigned int				count;
    char					*buffer;
    unsigned int				len;
};

/* prototypes */

int send_sftp_init(struct sftp_subsystem_s *sftp);

unsigned char get_sftp_protocol_version(struct sftp_subsystem_s *sftp);
void set_sftp_protocol_version(struct sftp_subsystem_s *sftp, unsigned char version);

void setup_sftp_idmapping(struct sftp_subsystem_s *sftp);

int init_sftp_subsystem(struct sftp_subsystem_s *sftp);
void free_sftp_subsystem(struct sftp_subsystem_s *sftp);
void finish_sftp_subsystem(struct sftp_subsystem_s *sftp);

#endif

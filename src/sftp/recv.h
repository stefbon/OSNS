/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _SFTP_RECV_COMMON_H
#define _SFTP_RECV_COMMON_H

#include "recv/recv-v03.h"
#include "recv/recv-v04.h"
#include "recv/recv-v05.h"
#include "recv/recv-v06.h"

void receive_sftp_data(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t seq, unsigned int flags);
void init_sftp_receive(struct sftp_client_s *sftp);
void switch_sftp_receive(struct sftp_client_s *sftp, const char *what);

void set_sftp_recv_version(struct sftp_client_s *sftp);

#endif

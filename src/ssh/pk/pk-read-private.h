/*
  2018 Stef Bon <stefbon@gmail.com>

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

#ifndef SSH_PK_READ_PRIVATE_H
#define SSH_PK_READ_PRIVATE_H

int read_skey_rsa(struct ssh_key_s *skey, char *buffer, unsigned int size, unsigned int format, unsigned int *error);
int read_skey_dss(struct ssh_key_s *skey, char *buffer, unsigned int size, unsigned int format, unsigned int *error);
int read_skey_ecc(struct ssh_key_s *skey, char *buffer, unsigned int size, unsigned int format, unsigned int *error);

void msg_read_skey_rsa(struct msg_buffer_s *mb, struct ssh_key_s *skey, unsigned int format);
void msg_read_skey_dss(struct msg_buffer_s *mb, struct ssh_key_s *skey, unsigned int format);
void msg_read_skey_ecc(struct msg_buffer_s *mb, struct ssh_key_s *skey, unsigned int format);

int read_skey(struct ssh_key_s *skey, char *buffer, unsigned int size, unsigned int format, unsigned int *error);

#endif

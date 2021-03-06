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

struct sftp_protocolextension_s *register_sftp_protocolextension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data,
				    void (* event_cb)(struct ssh_string_s *name, struct ssh_string_s *data, void *ptr, unsigned int event), void *ptr2);

struct sftp_protocolextension_s *add_sftp_protocolextension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data);

void complete_sftp_protocolextensions(struct sftp_client_s *sftp, char *mapextensionname);
void init_fs_sftp_extensions(struct sftp_client_s *sftp);

int send_sftp_extension_ctx(void *ptr, char *data, unsigned int size, struct sftp_reply_s *reply, unsigned int *error);
int send_sftp_extension_compat_ctx(void *ptr, struct ssh_string_s *name, struct ssh_string_s *data, struct sftp_reply_s *reply, unsigned int *error);

int send_sftp_fsync(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int *error);
int send_sftp_statvfs(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int *error);
void set_sftp_statvfs_unsupp(struct sftp_client_s *sftp);
void set_sftp_fsync_unsupp(struct sftp_client_s *sftp);

#endif

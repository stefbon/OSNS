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

#ifndef _FUSE_SFTP_PATH_H
#define _FUSE_SFTP_PATH_H

void _sftp_path_getattr(struct service_context_s *ctx, struct fuse_path_s *fpath, unsigned int mask, unsigned int property, const char *what,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_path_open(struct service_context_s *ctx, struct fuse_path_s *fpath, struct system_stat_s *stat, unsigned int flags, const char *what,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_path_mkdir(struct service_context_s *ctx, struct fuse_path_s *fpath, struct system_stat_s *stat,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_path_rm(struct service_context_s *ctx, struct fuse_path_s *fpath, const char *what,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_path_setattr(struct service_context_s *ctx, struct fuse_path_s *fpath, struct system_stat_s *stat,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

#endif

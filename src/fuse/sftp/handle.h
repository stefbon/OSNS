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

#ifndef _FUSE_SFTP_HANDLE_H
#define _FUSE_SFTP_HANDLE_H

void _sftp_handle_fgetattr(struct fuse_handle_s *handle, unsigned int mask, unsigned int property,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_fsetattr(struct fuse_handle_s *handle, struct system_stat_s *stat2set,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_fsync(struct fuse_handle_s *handle, unsigned int flags,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_release(struct fuse_handle_s *handle);

void _sftp_handle_fstatat(struct fuse_handle_s *handle, struct fuse_path_s *fpath, unsigned int mask, unsigned int property, unsigned int flags,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_pread(struct fuse_handle_s *handle, size_t size, off_t off,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_pwrite(struct fuse_handle_s *handle, const char *buff, size_t size, off_t off,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_lock(struct fuse_handle_s *handle, off_t off, unsigned int size, unsigned int blockmask,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_unlock(struct fuse_handle_s *handle, off_t off, unsigned int size,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void _sftp_handle_readdir(struct fuse_handle_s *handle, size_t size, off_t off,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr);


#endif

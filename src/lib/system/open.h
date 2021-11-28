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

#ifndef LIB_SYSTEM_OPEN_H
#define LIB_SYSTEM_OPEN_H

#include "path.h"

struct fs_socket_s {
#ifdef __linux__
    pid_t						pid;
    int							fd;
#endif
};

struct fs_init_s {
#ifdef __linux__
    mode_t						mode;
#endif
};

#define FSYNC_FLAG_DATASYNC				1

/* Prototypes */

int get_unix_fd_fs_socket(struct fs_socket_s *s);
unsigned int get_unix_pid_fs_socket(struct fs_socket_s *s);
void set_unix_fd_fs_socket(struct fs_socket_s *s, int fd);

void init_fs_socket(struct fs_socket_s *s);
int compare_fs_socket(struct fs_socket_s *s, struct fs_socket_s *t);

int system_open(struct fs_location_path_s *path, unsigned int flags, struct fs_socket_s *sock);
int system_create(struct fs_location_path_s *path, unsigned int flags, struct fs_init_s *init, struct fs_socket_s *sock);

int system_openat(struct fs_socket_s *ref, const char *name, unsigned int flags, struct fs_socket_s *sock);
int system_creatat(struct fs_socket_s *ref, const char *name, unsigned int flags, struct fs_init_s *init, struct fs_socket_s *sock);

int system_pread(struct fs_socket_s *s, char *data, unsigned int size, off_t off);
int system_pwrite(struct fs_socket_s *s, char *data, unsigned int size, off_t off);
int system_fsync(struct fs_socket_s *s);
int system_fdatasync(struct fs_socket_s *s);
int system_flush(struct fs_socket_s *s, unsigned int flags);
off_t system_lseek(struct fs_socket_s *s, off_t off, int whence);

void system_close(struct fs_socket_s *s);

int system_unlinkat(struct fs_socket_s *ref, const char *name);
int system_rmdirat(struct fs_socket_s *ref, const char *name);
int system_readlinkat(struct fs_socket_s *ref, const char *name, struct fs_location_path_s *target);

#endif

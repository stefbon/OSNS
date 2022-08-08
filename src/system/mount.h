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

#ifndef OSNS_SYSTEM_MOUNT_H
#define OSNS_SYSTEM_MOUNT_H

#include "lib/fuse/receive.h"

#define OSNS_SYSTEM_FUSE_BUFFER_SIZE		4096

#define OSNS_MOUNT_STATUS_INIT			(1 << 0)
#define OSNS_MOUNT_STATUS_OPEN			(1 << 1)
#define OSNS_MOUNT_STATUS_MOUNTED		(1 << 2)
#define OSNS_MOUNT_STATUS_NAMESPACE		(1 << 3)

struct osns_mount_s {
    unsigned int				status;
    unsigned char				type;
    struct list_element_s			list;
    struct shared_signal_s			*signal;
    struct system_socket_s			sock;
    char					*buffer;
    struct fuse_receive_s			receive;
};

/* prototypes */

struct osns_mount_s *mount_fuse_filesystem(struct osns_systemconnection_s *sc, struct shared_signal_s *signal, unsigned char type, unsigned int maxread, unsigned int *p_status);
int umount_fuse_filesystem(struct osns_systemconnection_s *sc, struct shared_signal_s *signal, unsigned char type);
void umount_all_fuse_filesystems(struct osns_systemconnection_s *sc, struct shared_signal_s *signal);

void umount_one_fuse_fs(struct osns_systemconnection_s *sc, struct osns_mount_s *om);

unsigned char test_mountpoint_is_osns_filesystem(struct fs_location_path_s *path);

#endif

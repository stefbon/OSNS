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

#ifndef LIB_MOUNTINFO_MONITOR_H
#define LIB_MOUNTINFO_MONITOR_H

#include "libosns-list.h"
#include "libosns-socket.h"

#define MOUNT_MONITOR_DEFAULT_BUFFER_SIZE		1024
#define MOUNT_MONITOR_FLAG_IGNORE_PSEUDOFS		1

#define MOUNTENTRY_STATUS_FREE				1
#define MOUNTENTRY_STATUS_LOCK				2

struct mountentry_s {
    uint64_t 				found;
    uint64_t 				generation;
    unsigned char			refcount;
    unsigned int			status;
    struct system_timespec_s		created;
    struct mount_monitor_s		*monitor;
    char 				*mountpoint;
    char 				*rootpath;
    char 				*fs;
    char 				*source;
    char 				*options;
    unsigned char 			flags;
    struct list_element_s		list;
    unsigned int			mountid;
    unsigned int			parentid;
    unsigned int			major;
    unsigned int			minor;
    void				(* free)(struct mountentry_s *me);
};

#define MOUNTMONITOR_ACTION_ADDED			1
#define MOUNTMONITOR_ACTION_REMOVED			2

#define MOUNT_MONITOR_STATUS_INIT			1
#define MOUNT_MONITOR_STATUS_MOUNTEVENT			2
#define MOUNT_MONITOR_STATUS_GENERATION			4
#define MOUNT_MONITOR_STATUS_PROCESS			8
#define MOUNT_MONITOR_STATUS_CHANGED			16

struct mount_monitor_s {
    unsigned int				status;
    unsigned int				flags;
    uint64_t					generation;
    struct shared_signal_s			*signal;
    int 					(*update) (unsigned char what, struct mountentry_s *me);
    unsigned char 				(*ignore) (char *source, char *fs, char *path, void *data);
    struct system_socket_s			sock;
    void					*data;
    struct list_header_s			mountentries;
    struct list_header_s			removedentries;
    char					*buffer;
    unsigned int				size;
};

/* prototypes */

struct mount_monitor_s *get_default_mount_monitor();

struct bevent_s *open_mountmonitor(struct shared_signal_s *signal, void *ptr, unsigned int flags);
void close_mountmonitor();
FILE *fopen_mountmonitor();

void read_mounttable();

void set_mount_monitor_ignore_cb(struct mount_monitor_s *monitor, unsigned char (* ignore_cb) (char *source, char *fs, char *path, void *data));
void set_mount_monitor_update_cb(struct mount_monitor_s *monitor, int (* update_cb) (unsigned char what, struct mountentry_s *me));

#endif

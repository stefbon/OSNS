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

#ifndef _LIB_EVENTLOOP_BACKEND_FSNOTIFY_H
#define _LIB_EVENTLOOP_BACKEND_FSNOTIFY_H

#define FSNOTIFY_SUBSCRIBER_FLAG_LOCK				1

#define FSNOTIFY_BACKEND_FLAG_ALLOC				1
#define FSNOTIFY_BACKEND_FLAG_LOCK_WATCHES			2

struct fsnotify_backend_s {
    unsigned int					flags;
    struct bevent_s					*bevent;
    struct shared_signal_s				*signal;
    struct list_header_s				subscribers;
    unsigned int					size;
    char						buffer[];
};

struct fsnotify_subscriber_s {
    unsigned int					flags;
    struct list_header_s				watches;
    struct list_element_s				list;
    struct fsnotify_user_s				user;
};

struct fswatch_path_s {
    struct fs_location_path_s				path;
    uint64_t						index;
    unsigned char					count;
    unsigned int					mask;
};

struct fswatch_s {
    struct system_timespec_s				created;
    struct fswatch_path_s				path;
    struct list_element_s 				list_b;
    struct list_header_s				subscribtions;
};

struct fswatch_subscribtion_s {
    struct list_element_s				list_w; /* per watch */
    struct list_element_s				list_s; /* per subscriber */
    struct fswatch_user_s				userswatch;
};


/* Prototypes */

struct fswatch_s *create_fswatch_backend(struct fswatch_path_s *path, unsigned int mask);


#endif

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

#ifndef LIB_SYSTEM_PATH_H
#define LIB_SYSTEM_PATH_H

#define FS_LOCATION_PATH_FLAG_ALLOC				1
#define FS_LOCATION_PATH_FLAG_PTR_ALLOC				2

struct fs_location_path_s {
#ifdef __linux__
    unsigned int						flags;
    char							*ptr;
    unsigned int						len;
    unsigned int						size;
#endif
};

#ifdef __linux__

#define FS_LOCATION_PATH_INIT					{0, NULL, 0, 0}
#define FS_LOCATION_PATH_SET(a, b)				{.flags=0, .size=(a ? a : strlen(b) + 1), .len=0, .ptr=b}

#endif

/* prototypes */

unsigned int append_location_path_get_required_size(struct fs_location_path_s *path, const unsigned char type, void *ptr);

void set_buffer_location_path(struct fs_location_path_s *path, char *buffer, unsigned int size, unsigned int flags);
void clear_location_path(struct fs_location_path_s *path);

unsigned int combine_location_path(struct fs_location_path_s *result, struct fs_location_path_s *path, const unsigned char type, void *ptr);
unsigned int append_location_path(struct fs_location_path_s *result, const unsigned char type, void *ptr);

int compare_location_paths(struct fs_location_path_s *a, struct fs_location_path_s *b); 
int compare_location_path(struct fs_location_path_s *path, char *extra, const unsigned char type, void *ptr);

unsigned int get_unix_location_path_length(struct fs_location_path_s *path);
unsigned int copy_unix_location_path(struct fs_location_path_s *path, char *buffer, unsigned int size);

char *get_filename_location_path(struct fs_location_path_s *path);
void detach_filename_location_path(struct fs_location_path_s *path, struct ssh_string_s *filename);

unsigned char test_location_path_absolute(struct fs_location_path_s *path);
unsigned char test_location_path_subdirectory(struct fs_location_path_s *path, const unsigned char type, void *ptr);

#endif

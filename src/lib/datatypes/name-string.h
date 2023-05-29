/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_DATATYPES_NAME_STRING_H
#define LIB_DATATYPES_NAME_STRING_H

#define NAME_STRING_INIT					{.len=0, .ptr=NULL}
#define NAME_STRING_SET(a, b)					{.len=(a ? a : strlen(b)), .ptr=b}

struct name_string_s {
    unsigned int			len;
    char				*ptr;
};

/* prototypes */

void init_name_string(struct name_string_s *s);
void set_name_string(struct name_string_s *s, const unsigned char type, char *ptr);

int compare_name_string(struct name_string_s *t, const unsigned char type, void *ptr);

unsigned int read_name_string(char *buffer, unsigned int size, struct name_string_s *s);
unsigned int write_name_string(char *buffer, unsigned int size, const unsigned char type, void *ptr);

#define COPY_NAME_STRING_FLAG_ALLOW_TRUNCATE				1

unsigned int copy_name_string(struct name_string_s *s, const unsigned char type, void *ptr, unsigned int flags);

#endif

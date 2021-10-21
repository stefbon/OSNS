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

#ifndef LIB_DATATYPES_NAME_H
#define LIB_DATATYPES_NAME_H

struct name_s {
    char 				*name;
    size_t				len;
    unsigned long long			index;
};

#define INIT_NAME			{NULL, 0, 0}

/* prototypes */

void init_name(struct name_s *a);
void set_name(struct name_s *a, char *s, unsigned int len);
void set_name_from(struct name_s *a, unsigned char type, void *ptr);
int compare_names(struct name_s *a, struct name_s *b);
void calculate_nameindex(struct name_s *name);

#endif

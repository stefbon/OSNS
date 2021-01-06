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

#ifndef _SSH_HASH_HASH_H
#define _SSH_HASH_HASH_H

#define SSH_HASH_TYPE_MD5			1
#define SSH_HASH_TYPE_SHA1			2
#define SSH_HASH_TYPE_SHA256			3
#define SSH_HASH_TYPE_SHA512			4
#define SSH_HASH_TYPE_SHA3_256			5
#define SSH_HASH_TYPE_SHA3_512			6

struct ssh_hash_s {
    char					name[32];
    union {
	int					algo;
    } lib;
    unsigned int				size;
    unsigned int				len;
    unsigned char				digest[];
};

/* prototypes */

unsigned int get_hash_size(const char *name);
void init_ssh_hash(struct ssh_hash_s *hash, char *name, unsigned int size);
unsigned int create_hash(char *in, unsigned int size, struct ssh_hash_s *hash, unsigned int *error);

#endif

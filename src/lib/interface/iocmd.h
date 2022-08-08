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

#ifndef _LIB_INTERFACE_IOCMD_H
#define _LIB_INTERFACE_IOCMD_H

#include "libosns-datatypes.h"

#define _IO_OPTION_TYPE_INT				1
#define _IO_OPTION_TYPE_PCHAR				2
#define _IO_OPTION_TYPE_PVOID				3
#define _IO_OPTION_TYPE_BUFFER				4

#define _IO_OPTION_FLAG_ERROR				1
#define _IO_OPTION_FLAG_ALLOC				2
#define _IO_OPTION_FLAG_NOTDEFINED			4
#define _IO_OPTION_FLAG_DEFAULT				8
#define _IO_OPTION_FLAG_VALID				16

struct io_option_s {
    unsigned char					type;
    unsigned char					flags;
    union {
	unsigned int					integer;
	char						*name;
	void						*ptr;
	struct io_option_buffer_s {
	    char					*ptr;
	    unsigned int				size;
	    unsigned int				len;
	} buffer;
    } value;
    void						(* free)(struct io_option_s *o);
};

/* prototypes */

void init_io_option(struct io_option_s *option, unsigned char type);
const char *get_name_interface_signal_sender(unsigned int type);

#endif

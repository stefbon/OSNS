/*
  2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_ERROR_ERROR_HANDLING_H
#define _LIB_ERROR_ERROR_HANDLING_H

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <syslog.h>
#include <unistd.h>
#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>

#include "log.h"

#define _ERROR_TYPE_SYSTEM				1
#define _ERROR_TYPE_APPLICATION				2
#define _ERROR_TYPE_LIBRARY				3

#define _ERROR_APPLICATION_TYPE_UNKNOWN			0
#define _ERROR_APPLICATION_TYPE_PROTOCOL		1
#define _ERROR_APPLICATION_TYPE_NOTFOUND		2

struct generic_error_s {
    unsigned char					type;
    union _errorcode_s {
	uint64_t					errnum;
	void						*ptr;
	char						buffer[8];
    } value;
    char						function[128];
    char						*(*get_description)(struct generic_error_s *error);
};

extern struct generic_error_s				initge;
#define GENERIC_ERROR_INIT				initge

/* prototypes */

char *get_error_description(struct generic_error_s *error);
void init_generic_error(struct generic_error_s *error);
void set_generic_error_application(struct generic_error_s *error, int errnum, char *(* get_desc)(struct generic_error_s *e), const char *function);

#ifdef __linux__

/* linux (and other x'ses) use a unsigned integer to indicate the system error */

void set_generic_error_system(struct generic_error_s *error, int errnum, const char *function);

#endif

#endif

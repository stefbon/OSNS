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

#ifndef _UTILS_REPLACEANDSKIP_H
#define _UTILS_REPLACEANDSKIP_H

#define SKIPSPACE_FLAG_REPLACEBYZERO		1

#define REPLACE_CNTRL_FLAG_TEXT			1
#define REPLACE_CNTRL_FLAG_BINARY		2
#define REPLACE_CNTRL_FLAG_UNDERSCORE		4

/* prototypes */

void replace_cntrl_char(char *buffer, unsigned int size, unsigned char flag);
void replace_slash_char(char *buffer, unsigned int size);
void replace_newline_char(char *ptr, unsigned int size);
unsigned int skip_trailing_spaces(char *ptr, unsigned int size, unsigned int flags);
unsigned int skip_heading_spaces(char *ptr, unsigned int size);

#endif

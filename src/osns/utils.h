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

#ifndef OSNS_UTILS_H
#define OSNS_UTILS_H

#include "libosns-datatypes.h"

/* prototypes */

char *get_osns_service_name(uint16_t service);

unsigned int create_osns_version(unsigned int major, unsigned int minor);
unsigned int get_osns_major(unsigned int version);
unsigned int get_osns_minor(unsigned int version);

unsigned int get_osns_protocol_flags(struct osns_connection_s *oc);

#endif

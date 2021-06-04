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

#ifndef OSNS_LIB_DISCOVER_AVAHI_H
#define OSNS_LIB_DISCOVER_AVAHI_H

#define DISCOVER_AVAHI_FLAG_ALLOW_LOCALHOST			1

/* Prototypes */

void browse_services_avahi(unsigned int flags, void (* cb)(const char *name, const char *hostname, char *ipv4, const char *domain, unsigned int port, const char *type));
void stop_browse_avahi();

#endif

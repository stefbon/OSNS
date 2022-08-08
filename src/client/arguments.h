/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017  Stef Bon <stefbon@gmail.com>

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

#ifndef CLIENT_OSNS_OPTIONS_H
#define CLIENT_OSNS_OPTIONS_H

#include "osns-protocol.h"

#define OSNS_CLIENT_CONFIGFILE					"/etc/osns/client.conf"

/* VARIOUS */

#define CLIENT_ARGUMENTS_FLAG_DEFAULT_CONFIGFILE		(1 << 5)

struct client_arguments_s {
    unsigned int			flags;
    char				*configfile;
};

/* Prototypes */

int parse_arguments(int argc, char *argv[], struct client_arguments_s *arguments);
void free_arguments(struct client_arguments_s *arguments);

#endif

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

#ifndef CTL_OSNS_OPTIONS_H
#define CTL_OSNS_OPTIONS_H

#include "osns-protocol.h"

struct ctl_arguments_s {
    unsigned int				type;
    unsigned int				init;
    union _argument_type_u {
	struct mount_argument_s {
	    unsigned int			type;
	} mount;
	struct list_argument_s {
	    unsigned int			service;
	    struct osns_record_s		filter;
	} list;
	struct watch_argument_s {
	    unsigned int			type;
	    struct osns_record_s		target;
	} watch;
	struct channel_argument_s {
	    unsigned int			type;
	    struct osns_record_s		host;
	    struct osns_record_s		command;
	} channel;
    } cmd;
};

/* Prototypes */

int read_osnsctl_arguments(int argc, char *argv[], struct ctl_arguments_s *arguments);
void free_osnsctl_arguments(struct ctl_arguments_s *arguments);

#endif

/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "connection.h"

void free_bevent_hlpr(struct system_socket_s *sock)
{
    struct bevent_s *bevent=sock->event.link.bevent;

    if (bevent) {

	remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);
	free_bevent(&bevent);
	sock->event.link.bevent=NULL;

    }

}

void close_socket_hlpr(int fd, struct system_socket_s *sock, unsigned char free)
{

    if ((fd==-1) || (fd==(* sock->sops.get_unix_fd)(sock))) {

	(* sock->sops.close)(sock);
	(* sock->sops.set_unix_fd)(sock, -1);
	if (free) free_bevent_hlpr(sock);

    }

}

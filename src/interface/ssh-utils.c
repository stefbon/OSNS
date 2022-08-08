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
#include "libosns-workspace.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-context.h"
#include "libosns-socket.h"

#include "ssh/ssh-common.h"
#include "ssh/ssh-common-client.h"
#include "ssh/ssh-common.h"

struct connection_s *get_connection_ssh_interface(struct context_interface_s *i)
{

    if (i->type==_INTERFACE_TYPE_SSH_SESSION) {
	char *buffer=(* i->get_interface_buffer)(i);
	struct ssh_session_s *s=(struct ssh_session_s *) buffer;

	return get_session_connection(s);

    }

    return NULL;
}

unsigned int get_default_ssh_port(struct context_interface_s *i)
{
    unsigned int port=0;

    if (i->type==_INTERFACE_TYPE_SSH_SESSION) {
	char *buffer=(* i->get_interface_buffer)(i);
	struct ssh_session_s *s=(struct ssh_session_s *) buffer;

	port=s->config.port;

    }

    return port;
}

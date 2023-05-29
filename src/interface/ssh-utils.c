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

#include "fuse.h"

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

struct beventloop_s *get_workspace_beventloop(struct context_interface_s *i)
{
    struct service_context_s *ctx=get_service_context(i);
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct list_element_s *l1=NULL;

    l1=get_list_head(&w->contexes);

    if (l1) {
        struct service_context_s *tmp=(struct service_context_s *)((char *) l1 - offsetof(struct service_context_s, wlist));

        if (tmp->interface.type==_INTERFACE_TYPE_FUSE) {
            struct beventloop_s *loop=get_fuse_interface_eventloop(&tmp->interface);

            if (loop) return loop;

        }

    }

    l1=get_list_head(&w->shared_contexes);

    while (l1) {
        struct service_context_s *tmp=(struct service_context_s *)((char *) l1 - offsetof(struct service_context_s, wlist));
        struct list_header_s *h=(* tmp->interface.get_header_connections)(&tmp->interface);
        struct list_element_s *l2=NULL;

        l2=get_list_head(h);

        while (l2) {
            struct connection_s *c=(struct connection_s *)((char *) l2 - offsetof(struct connection_s, list));
            struct beventloop_s *loop=osns_socket_get_eventloop(&c->sock);

            if (loop) return loop;
            l2=get_next_element(l2);

        }

        l1=get_next_element(l1);

    }

    return NULL;

}

struct beventloop_s *get_ssh_session_eventloop(struct context_interface_s *i)
{

    if (i->type==_INTERFACE_TYPE_SSH_SESSION) {
	char *buffer=(* i->get_interface_buffer)(i);
	struct ssh_session_s *s=(struct ssh_session_s *) buffer;
	struct connection_s *c=get_session_connection(s);

        return (c ? osns_socket_get_eventloop(&c->sock) : NULL);

    }

    return NULL;

}

struct shared_signal_s *get_workspace_signal(struct context_interface_s *i)
{
    struct service_context_s *ctx=get_service_context(i);
    struct service_context_s *root=get_root_context(ctx);
    return (root) ? root->service.workspace.signal : NULL;
}

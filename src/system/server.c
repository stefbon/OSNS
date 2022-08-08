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
#include "libosns-network.h"
#include "libosns-connection.h"
#include "libosns-eventloop.h"
#include "libosns-mountinfo.h"
#include "libosns-threads.h"

#include "lib/system/stat.h"
#include "lib/system/fsoperations.h"

#include "osns-protocol.h"
#include "osns_system.h"
#include "osns/pidfile.h"

#include "system/receive.h"

static struct connection_s *osns_server=NULL;

static void check_socketpath_does_exist(struct fs_location_path_s *path)
{
    struct system_stat_s stat;

    if (system_getstat(path, SYSTEM_STAT_TYPE, &stat)==0) {

	if (system_stat_test_ISSOCK(&stat)) {

	    logoutput("check_socketpath_does_exist: existing socket found ... removing");
	    system_remove_file(path);

	}

    }

}


int create_local_socket(struct connection_s *server, char *runpath, char *group)
{
    struct fs_location_path_s rundir=FS_LOCATION_PATH_INIT;
    unsigned int size=0;
    int result=-1;

    if (server==NULL) return -1;
    logoutput("create_local_socket: creating local socket in %s", runpath);
    osns_server=server;

    init_connection(server, CONNECTION_TYPE_LOCAL, CONNECTION_ROLE_SOCKET, 0);

    set_location_path(&rundir, 'c', runpath);
    size=append_location_path_get_required_size(&rundir, 'c', "system.sock") + 8;

    if (size>0) {
	char buffer[size];
	struct fs_location_path_s socket=FS_LOCATION_PATH_INIT;
	struct connection_address_s address;

	assign_buffer_location_path(&socket, buffer, size);
	combine_location_path(&socket, &rundir, 'c', "system.sock");
	address.target.path=&socket;

	check_socketpath_does_exist(&socket);
	result=create_serversocket(server, NULL, accept_connection_from_localsocket, &address, NULL);

	if (result==0) {
	    struct socket_properties_s prop;

	    memset(&prop, 0, sizeof(struct socket_properties_s));

#ifdef __linux__

	    prop.valid=(SOCKET_PROPERTY_FLAG_GROUP | SOCKET_PROPERTY_FLAG_MODE);
	    prop.group=group;
	    prop.mode=enable_mode_permission(0, STAT_MODE_ROLE_USER | STAT_MODE_ROLE_GROUP, STAT_MODE_PERM_READ | STAT_MODE_PERM_WRITE);

#endif
	    set_socket_properties(&server->sock, &prop);

	    logoutput_debug("create_local_socket: B");

	}

    }

    logoutput_debug("create_local_socket: C %i", result);
    return result;
}

void clear_local_connections()
{
    if (osns_server==NULL) return;

    if (osns_server->ops.server.header.count>0) {
	struct list_element_s *list=NULL;

	list=get_list_head(&osns_server->ops.server.header, SIMPLE_LIST_FLAG_REMOVE);

	while (list) {
	    struct connection_s *c=(struct connection_s *)((char *) list - offsetof(struct connection_s, list)); /* list -> connection */
	    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *) c - offsetof(struct osns_systemconnection_s, connection)); /* connection -> local connection*/
	    struct system_socket_s *sock=&c->sock;

	    if (sock->event.type==SOCKET_EVENT_TYPE_BEVENT) {
		struct bevent_s *bevent=sock->event.link.bevent;

		remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);
		free_bevent(&bevent);
		sock->event.link.bevent=NULL;
		sock->event.type=0;

	    }

	    (* sock->sops.close)(sock);
	    clear_connection(c);
	    clear_systemconnection(sc);
	    free(sc);

	    list=get_list_head(&osns_server->ops.server.header, SIMPLE_LIST_FLAG_REMOVE);

	}

    }

}

void close_osns_server()
{

    if (osns_server) {

	struct system_socket_s *sock=&osns_server->sock;
	(* sock->sops.close)(sock);

    }

}

void clear_osns_server()
{
    if (osns_server) clear_connection(osns_server);
}

struct connection_s *get_client_connection(uint64_t unique)
{
    struct list_header_s *h=NULL;
    struct list_element_s *list=NULL;
    struct connection_s *c=NULL;

    if (osns_server==NULL) return NULL;
    h=&osns_server->ops.server.header;

    read_lock_list_header(h);
    list=get_list_head(h, 0);

    while (list) {

	c=(struct connection_s *) ((char *) list - offsetof(struct connection_s, list));
	if (c->ops.client.unique==unique) break;
	list=get_next_element(list);

    }

    read_unlock_list_header(h);
    return c;
}

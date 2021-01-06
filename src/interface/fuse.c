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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define LOGGING
#include "logging.h"

#include "main.h"
#include "options.h"
#include "workspace.h"
#include "eventloop.h"
#include "workspace-interface.h"
#include "fuse.h"

/* FUSE callbacks */

static int _connect_interface_fuse(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    int fd=-1;
    unsigned int error=0;

    if (!(service->type==_SERVICE_TYPE_FUSE)) {

	logoutput_warning("_connect_interface_fuse: error, service type wrong (service type=%i)", service->type);
	goto out;

    } else if (service->target.fuse.mountpoint==NULL || service->target.fuse.source==NULL || service->target.fuse.name==NULL) {

	logoutput_warning("_connect_interface_fuse: error, mountpoint, source and/or name not defined");
	goto out;

    }

    fd=connect_fusesocket(interface->buffer, uid, service->target.fuse.source, service->target.fuse.mountpoint, service->target.fuse.name, &error);

    out:
    return fd;
}

static int _start_interface_fuse(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    struct fs_connection_s *connection=get_fs_connection_fuse(interface->buffer);
    struct bevent_s *bevent=&connection->io.fuse.bevent;
    struct beventloop_s *loop=interface->backend.fuse.loop;

    if (add_to_beventloop(fd, BEVENT_CODE_IN, read_fusesocket_event, (void *) (interface->buffer), bevent, loop)) {

	logoutput("_start_interface_fuse: added fd %i to eventloop", fd);

    } else {

	logoutput("_start_interface_fuse: failed to add fd %i to eventloop", fd);
	return -1;

    }

    return 0;
}

static int _signal_interface(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    signal_ctx2fuse_t cb=get_signal_ctx2fuse(interface->buffer);
    void *ptr=(void *) interface->buffer;

    return (* cb)(&ptr, what, option);
}

static int _signal_fuse2ctx(void *ptr, const char *what, struct ctx_option_s *option)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) ptr - offsetof(struct context_interface_s, buffer));
    return (* interface->signal_context)(interface, what, option);
}

/*
	INTERFACE OPS
			*/

static unsigned int _populate_fuse_interface(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start)
{

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_FUSE;
	ilist[start].name="fuse";
	ilist[start].ops=ops;

    }

    start++;
    return start;
}

static unsigned int _get_interface_buffer_size_fuse(struct interface_list_s *ilist)
{
    logoutput("_get_interface_buffer_size_fuse");

    if (ilist->type==_INTERFACE_TYPE_FUSE) return (get_fuse_buffer_size());
    return 0;
}

static int _init_interface_buffer_fuse(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    struct fuse_user_s *user=workspace->user;
    struct interface_ops_s *ops=ilist->ops;

    logoutput("_init_interface_buffer_fuse");

    if (interface->size < (* ops->get_buffer_size)(ilist)) {

	logoutput_warning("_init_interface_buffer_fuse: buffer size too small (%i, required %i) cannot continue", interface->size, (* ops->get_buffer_size)(ilist));
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_fuse: buffer already initialized");
	return 0;

    }

    logoutput("_init_interface_buffer_fuse: A");

    interface->type=_INTERFACE_TYPE_FUSE;
    init_fusesocket(interface->buffer, (void *) interface, interface->size, FUSESOCKET_INIT_FLAG_SIZE_INCLUDES_SOCKET);

    logoutput("_init_interface_buffer_fuse: B");

    interface->connect=_connect_interface_fuse;
    interface->start=_start_interface_fuse;
    interface->signal_interface=_signal_interface;
    set_signal_fuse2ctx(interface->buffer, _signal_fuse2ctx);

    logoutput("_init_interface_buffer_fuse: ready");
    interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
    return 0;

}

static void _clear_interface_buffer(struct context_interface_s *interface)
{

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	if ((interface->flags & _INTERFACE_FLAG_BUFFER_CLEAR)==0) {

	    (* interface->signal_interface)(interface, "command:disconnect:", NULL);
	    (* interface->signal_interface)(interface, "command:close:", NULL);
	    (* interface->signal_interface)(interface, "command:free:", NULL);
	    interface->flags |= _INTERFACE_FLAG_BUFFER_CLEAR;

	}

	interface->flags -= _INTERFACE_FLAG_BUFFER_INIT;
	reset_context_interface(interface);

    }

}

static struct interface_ops_s fuse_interface_ops = {
    .name					= "FUSE",
    .populate					= _populate_fuse_interface,
    .get_buffer_size				= _get_interface_buffer_size_fuse,
    .init_buffer				= _init_interface_buffer_fuse,
    .clear_buffer				= _clear_interface_buffer,
};

void init_fusesocket_interface()
{
    add_interface_ops(&fuse_interface_ops);
}

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
#include "libosns-fuse.h"
#include "libosns-fuse-public.h"

#include "osns_client.h"
#include "client/workspaces.h"

#include "osns/send/mountcmd.h"

#define FUSE_HASHTABLE_SIZE			64

typedef void (* fuse_request_cb_t)(struct fuse_request_s *req, char *data);

#define FUSE_REQUEST_BUFFER_SIZE	        16384

struct fuse_interface_s {
    struct beventloop_s 			*loop;
    struct osns_socket_s			socket;
    struct fuse_config_s			config;
    struct system_dev_s                         dev;
    unsigned int				countcb;
    fuse_request_cb_t				cb[FUSE_REQUEST_CB_MAX+1];
    struct list_header_s			hashtable[FUSE_HASHTABLE_SIZE];
    struct fuse_receive_s			receive;
    unsigned int                                size;
    char					*buffer;
};

/* FUSE io functions */

static void fuse_fs_reply_error_enosys(struct fuse_request_s *req, char *data)
{
    reply_VFS_error(req, ENOSYS);
}

void add_fuse_request_hashtable(struct fuse_interface_s *fi, struct fuse_request_s *req)
{
    unsigned int hashvalue=(req->unique % FUSE_HASHTABLE_SIZE);
    struct list_header_s *h=&fi->hashtable[hashvalue];

    write_lock_list_header(h);
    add_list_element_last(h, &req->list);
    write_unlock_list_header(h);
}

void remove_fuse_request_hashtable(struct fuse_interface_s *fi, struct fuse_request_s *req)
{
    unsigned int hashvalue=(req->unique % FUSE_HASHTABLE_SIZE);
    struct list_header_s *h=&fi->hashtable[hashvalue];

    write_lock_list_header(h);
    remove_list_element(&req->list);
    write_unlock_list_header(h);
}

static void set_fuse_request_flags_default(struct fuse_request_s *req)
{
}

static void osns_client_process_fuse_data(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data)
{
    struct fuse_interface_s *fi=(struct fuse_interface_s *)((char *) r - offsetof(struct fuse_interface_s, receive));
    struct context_interface_s *interface=(struct context_interface_s *)((char *) fi - offsetof(struct context_interface_s, buffer));

    logoutput_debug("osns_client_process_fuse_data: code %u node id %lu unique %lu", inh->opcode, inh->nodeid, inh->unique);

    if (inh->opcode <= FUSE_REQUEST_CB_MAX) {
	struct fuse_request_s request;

	memset(&request, 0, sizeof(struct fuse_request_s));

	request.interface=interface;
	request.ptr=NULL;
	request.sock=&fi->socket;
	request.opcode=inh->opcode;
	request.ino=inh->nodeid;
	request.uid=inh->uid;
	request.gid=inh->gid;
	request.pid=inh->pid;
	request.unique=inh->unique;
	init_list_element(&request.list, NULL);
	request.flags=0;
	request.size=inh->len;

	add_fuse_request_hashtable(fi, &request);
	(* fi->cb[inh->opcode])(&request, data);
	remove_fuse_request_hashtable(fi, &request);

    } else {
	struct fuse_request_s request;

	memset(&request, 0, sizeof(struct fuse_request_s));
	request.interface=interface;
	request.sock=&fi->socket;
	request.unique=inh->unique;

	reply_VFS_error(&request, ENOSYS);

    }

}

static void _signal_fuse_request_interrupted(struct fuse_interface_s *fi, uint64_t unique)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) fi - offsetof(struct context_interface_s, buffer));
    struct service_context_s *ctx=get_service_context(interface);
    unsigned int hashvalue=(unique % FUSE_HASHTABLE_SIZE);
    struct list_header_s *h=&fi->hashtable[hashvalue];
    struct list_element_s *list=NULL;
    struct fuse_request_s *req=NULL;

    logoutput_debug("_signal_fuse_request_interrupted: unique %lu", unique);

    /* get read access to this header/list */

    read_lock_list_header(h);

    list=get_list_head(h);

    while (list) {

	req=(struct fuse_request_s *)((char *) list - offsetof(struct fuse_request_s, list));

	if ((req->unique==unique) && (req->interface==interface)) {

	    logoutput_debug("_signal_fuse_request_interrupted: req with unique %lu found", unique);
	    signal_set_flag(ctx->service.workspace.signal, &req->flags, FUSE_REQUEST_FLAG_INTERRUPTED);
	    break;

	}

	list=get_next_element(list);
	req=NULL;

    }

    read_unlock_list_header(h);

    if (req) {

	logoutput_debug("_signal_fuse_request_interrupted: req with unique %lu set", unique);

    } else {

	logoutput_debug("_signal_fuse_request_interrupted: unique %lu not found", unique);

    }

}

void signal_fuse_request_interrupted(struct context_interface_s *interface, uint64_t unique)
{
    struct fuse_interface_s *fi=(struct fuse_interface_s *) interface->buffer;
    _signal_fuse_request_interrupted(fi, unique);
}

static void register_fuse_function(struct context_interface_s *i, unsigned int ctr, unsigned int opcode, void (* cb) (struct fuse_request_s *r, char *d))
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;

    if (opcode>0 && opcode<=fuse->countcb) {

	fuse->cb[opcode]=cb;

    } else {

	logoutput_error("register_fuse_function: error opcode %i out of range", opcode);

    }

}

/* FUSE interface callbacks */

static int _connect_interface_fuse(struct context_interface_s *interface, union interface_target_u *target, union interface_parameters_u *param)
{
    struct service_context_s *ctx=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(ctx);
    struct client_session_s *session=NULL;
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) interface->buffer;
    struct fuse_mount_s *mount=target->fuse;
    int result=-1;
    unsigned int error=0;

    if (workspace==NULL) {

	logoutput_warning("_connect_interface_fuse: error, interface not part of workspace");
	goto out;

    }

    session=get_client_session_workspace(workspace);

    if (session==NULL) {

	logoutput_warning("_connect_interface_fuse: error, workspace not added to client session");
	goto out;

    }

    if (fuse->buffer==NULL) {
        void *buffer=NULL;

        buffer=malloc(FUSE_REQUEST_BUFFER_SIZE);

        //if (posix_memalign(&buffer, getpagesize(), FUSE_REQUEST_BUFFER_SIZE)==0) {

        if (buffer) {

            fuse->buffer=(char *) buffer;
            fuse->size=FUSE_REQUEST_BUFFER_SIZE;
            logoutput_debug("_connect_interface_fuse: allocated %u alligned memory", fuse->size);

        } else {

            logoutput_debug("_connect_interface_fuse: unable to allocate %u alligned memory", fuse->size);
            goto out;

        }

    }

    result=do_osns_mountcmd(&session->osns, mount->type, mount->maxread, &fuse->socket, &fuse->dev);

    if (result==1) {

	result=(* fuse->socket.get_unix_fd)(&fuse->socket);
	logoutput_debug("_connect_interface_fuse: received fd %i", result);

    } else if (result==0) {

	logoutput_debug("_connect_interface_fuse: no fd received");
	result=-1;

    } else if (result==-1) {

	logoutput_debug("_connect_interface_fuse: error");

    } else {

	logoutput_debug("_connect_interface_fuse: unknown result (%i) process mountcmd", result);

    }

    out:
    return result;
}

static int _start_interface_fuse(struct context_interface_s *interface)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) interface->buffer;
    struct osns_socket_s *sock=&fuse->socket;
    int result=-1;

    /* add to common eventloop */

    if (add_osns_socket_eventloop(sock, fuse->loop, (void *) &fuse->receive, 0)==0) {

        logoutput("_start_interface_fuse: added to eventloop");
        init_fuse_socket_ops(sock, fuse->buffer, FUSE_REQUEST_BUFFER_SIZE);
        result=0;

    } else {

        logoutput("_start_interface_fuse: failed to add to eventloop");

    }

    return result;

}

static int iocmd_ctx2fuse(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    const char *sender=get_name_interface_signal_sender(type);

    logoutput_debug("iocmd_ctx2fuse: %s by %s", ((what) ? what : "--notset--"), sender);

    if (what && strlen(what)>8 && strncmp(what, "command:", 8)==0) {
	unsigned int pos=8;

	if (strncmp(&what[pos], "close:", 6)==0) {

	    // osns_client_close_fuse_interface(fuse, NULL);
	    return 1;

	} else if (strncmp(&what[pos], "free:", 5)==0) {
	    struct osns_socket_s *sock=&fuse->socket;

	    /* free additional data allocated */

	    return 1;

	}

    }

    return 0;

}

static int iocmd_fuse2ctx(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type)
{
    const char *sender=get_name_interface_signal_sender(type);

    logoutput_debug("iocmd_fuse2ctx: %s by %s", ((what) ? what : "--notset--"), sender);
    /* what commands can come from the fuse interface to inform the context? */

    return 0;
}

static int notify_VFS_client_cb(struct fuse_receive_s *r, unsigned int code, struct iovec *iov, unsigned int count)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *)((char *) r - offsetof(struct fuse_interface_s, receive));
    struct osns_socket_s *sock=&fuse->socket;

    return fuse_socket_notify(sock, code, iov, count);
}

static int reply_VFS_client_cb(struct fuse_receive_s *r, uint64_t unique, char *data, unsigned int size)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *)((char *) r - offsetof(struct fuse_interface_s, receive));
    struct osns_socket_s *sock=&fuse->socket;

    return fuse_socket_reply_data(sock, unique, data, size);
}

static int error_VFS_client_cb(struct fuse_receive_s *r, uint64_t unique, unsigned int code)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *)((char *) r - offsetof(struct fuse_interface_s, receive));
    struct osns_socket_s *sock=&fuse->socket;

    return fuse_socket_reply_error(sock, unique, code);
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

static unsigned int _get_interface_buffer_size_fuse(struct interface_list_s *ilist, struct context_interface_s *p)
{
    logoutput("_get_interface_buffer_size_fuse");

    if (ilist->type==_INTERFACE_TYPE_FUSE) {

	return (sizeof(struct fuse_interface_s) + FUSE_REQUEST_BUFFER_SIZE);

    }

    return 0;
}

static int _init_interface_buffer_fuse(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct interface_ops_s *ops=ilist->ops;
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) interface->buffer;

    logoutput("_init_interface_buffer_fuse");

    if (interface->size < (* ops->get_buffer_size)(ilist, NULL)) {

	logoutput_warning("_init_interface_buffer_fuse: buffer size too small (%i, required %i) cannot continue", interface->size, (* ops->get_buffer_size)(ilist, NULL));
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_fuse: buffer already initialized");
	return 0;

    }

    memset(fuse, 0, sizeof(struct fuse_interface_s));
    fuse->loop=NULL; /* to be set later */

    init_osns_socket(&fuse->socket, OSNS_SOCKET_TYPE_DEVICE, (OSNS_SOCKET_FLAG_RDWR | OSNS_SOCKET_FLAG_CHAR_DEVICE));

    /* default configuration */

    fuse->config.flags = (FUSE_CONFIG_FLAG_SYMLINKS_ALLOW_PREFIX | FUSE_CONFIG_FLAG_HIDE_SYSTEMFILES);
    set_default_fuse_timeout(&fuse->config.attr_timeout, 0);
    set_default_fuse_timeout(&fuse->config.entry_timeout, 0);
    set_default_fuse_timeout(&fuse->config.neg_timeout, 0);

    fuse->countcb=FUSE_REQUEST_CB_MAX;
    for (unsigned int i=0; i<=fuse->countcb; i++) fuse->cb[i]=fuse_fs_reply_error_enosys;
    register_fuse_functions(interface, register_fuse_function);
    for (unsigned int i=0; i<FUSE_HASHTABLE_SIZE; i++) init_list_header(&fuse->hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);

    fuse->receive.status=0;
    fuse->receive.flags=0;
    fuse->receive.ptr=NULL;
    fuse->receive.loop=((fuse->loop) ? (fuse->loop) : get_default_mainloop());
    fuse->receive.signal=workspace->signal; /* use the shared signal for this workspace */

    fuse->receive.process_data=osns_client_process_fuse_data;
    fuse->receive.notify_VFS=notify_VFS_client_cb;
    fuse->receive.reply_VFS=reply_VFS_client_cb;
    fuse->receive.error_VFS=error_VFS_client_cb;
    fuse->receive.sock=&fuse->socket;

    fuse->buffer=NULL;
    fuse->size=0;

    interface->type=_INTERFACE_TYPE_FUSE;
    interface->connect=_connect_interface_fuse;
    interface->start=_start_interface_fuse;
    interface->iocmd.in=iocmd_ctx2fuse;
    interface->iocmd.out=iocmd_fuse2ctx;
    interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;

    return 0;

}

static void _clear_interface_buffer(struct context_interface_s *i)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;

    if (fuse->buffer) {

        free(fuse->buffer);
        fuse->buffer=NULL;

    }

    clear_interface_buffer_default(i);

}

static struct interface_ops_s fuse_interface_ops = {
    .name					= "FUSE",
    .populate					= _populate_fuse_interface,
    .get_buffer_size				= _get_interface_buffer_size_fuse,
    .init_buffer				= _init_interface_buffer_fuse,
    .clear_buffer				= _clear_interface_buffer,
};

void init_fuse_interface()
{

    if (add_interface_ops(&fuse_interface_ops)) {

	init_virtual_fs();
	init_fuse_handle_hashtable();
	init_service_path_fs();
	init_service_browse_fs();

    }

}

void set_fuse_interface_eventloop(struct context_interface_s *i, struct beventloop_s *loop)
{
    if (i->type==_INTERFACE_TYPE_FUSE) {

        struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
        fuse->loop=loop;

    }

}

struct beventloop_s *get_fuse_interface_eventloop(struct context_interface_s *i)
{

    if (i->type==_INTERFACE_TYPE_FUSE) {
        struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
        return fuse->loop;

    }

    return NULL;
}

struct system_timespec_s *get_fuse_attr_timeout(struct context_interface_s *i)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    return &fuse->config.attr_timeout;
}

struct system_timespec_s *get_fuse_entry_timeout(struct context_interface_s *i)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    return &fuse->config.entry_timeout;
}

struct system_timespec_s *get_fuse_neg_timeout(struct context_interface_s *i)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    return &fuse->config.neg_timeout;
}

struct fuse_config_s *get_fuse_interface_config(struct context_interface_s *i)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    return &fuse->config;
}

struct system_dev_s *get_fuse_interface_system_dev(struct context_interface_s *i)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    return &fuse->dev;
}

int fuse_notify_VFS_delete(struct context_interface_s *i, uint64_t pino, uint64_t ino, char *name, unsigned int len)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    struct osns_socket_s *sock=&fuse->socket;
    struct iovec iov[3];
    struct fuse_notify_delete_out out;

    out.parent=pino;
    out.child=ino;
    out.namelen=len;
    out.padding=0;

    memset(iov, 0, 3 * sizeof(struct iovec));

    iov[1].iov_base=(void *) &out;
    iov[1].iov_len=sizeof(struct fuse_notify_delete_out);

    iov[2].iov_base=(void *) name;
    iov[2].iov_len=len;

    return fuse_socket_notify(sock, FUSE_NOTIFY_DELETE, iov, 3);

}

int fuse_reply_VFS_data(struct context_interface_s *i, uint64_t unique, char *data, unsigned int len)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    struct osns_socket_s *sock=&fuse->socket;

    return fuse_socket_reply_data(sock, unique, data, len);
}

int fuse_reply_VFS_error(struct context_interface_s *i, uint64_t unique, unsigned int errcode)
{
    struct fuse_interface_s *fuse=(struct fuse_interface_s *) i->buffer;
    struct osns_socket_s *sock=&fuse->socket;

    return fuse_socket_reply_error(sock, unique, errcode);
}

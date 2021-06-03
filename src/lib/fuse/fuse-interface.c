/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

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
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pthread.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "log.h"
#include "eventloop.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "threads.h"

#define FUSE_HASHTABLE_SIZE				128
static struct list_header_s				hashtable[FUSE_HASHTABLE_SIZE];
static unsigned char					hashinit=0;
static uint32_t						hashctr;
static pthread_mutex_t					hashmutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t					hashcond=PTHREAD_COND_INITIALIZER;

#define FUSE_FLAG_CONNECTING				1
#define FUSE_FLAG_CONNECTED				2
#define FUSE_FLAG_DISCONNECTING				4
#define FUSE_FLAG_DISCONNECTED				8
#define FUSE_FLAG_DISCONNECT				( FUSE_FLAG_DISCONNECTING | FUSE_FLAG_DISCONNECTED )
#define FUSE_FLAG_ALLOC					16
#define FUSE_FLAG_RECEIVE				32
#define FUSE_FLAG_WAITING1				64
#define FUSE_FLAG_WAITING2				128
#define FUSE_FLAG_WAIT					( FUSE_FLAG_WAITING1 | FUSE_FLAG_WAITING2 )
#define FUSE_FLAG_ERROR					256
#define FUSE_FLAG_CLEAR					512

#define FUSE_MAXNR_CB					47

/* actual connection to the VFS/kernel */

struct fusesocket_s;

struct fusesignal_s {
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
};

struct fusecontext_s {
    uint64_t						unique;
    void						*ctx;
    int							(* signal_fuse2ctx)(void *ptr, const char *what, struct ctx_option_s *option);
    int							(* signal_ctx2fuse)(void **p_ptr, const char *what, struct ctx_option_s *option);
};

struct fusesocket_s {
    unsigned int					flags;
    struct fusecontext_s				context;
    struct timespec					attr_timeout;
    struct timespec					entry_timeout;
    struct timespec					negative_timeout;
    struct fs_connection_s				connection;
    unsigned int					size_cb;
    fuse_cb_t						fuse_cb[FUSE_MAXNR_CB + 1]; /* depends on version protocol; at this moment max opcode is 47 */
    mode_t						(* get_masked_perm)(mode_t perm, mode_t mask);
    struct fusesignal_s					signal;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
    unsigned int					threads;
    size_t 						size;
    size_t						read;
    char						buffer[];
};


void notify_VFS_delete(char *ptr, uint64_t pino, uint64_t ino, char *name, unsigned int len)
{

#if FUSE_VERSION >= 29
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    struct fs_connection_s *conn=&fusesocket->connection;
    struct fuse_ops_s *fops=conn->io.fuse.fops;
    ssize_t alreadywritten=0;
    struct iovec iov[3];
    struct fuse_out_header oh;
    struct fuse_notify_delete_out out;

    oh.len=size_out_header + sizeof(struct fuse_notify_delete_out) + len;
    oh.error=FUSE_NOTIFY_DELETE;
    oh.unique=0;

    out.parent=pino;
    out.child=ino;
    out.namelen=len;
    out.padding=0;

    iov[0].iov_base=(void *) &oh;
    iov[0].iov_len=size_out_header;

    iov[1].iov_base=(void *) &out;
    iov[1].iov_len=sizeof(struct fuse_notify_delete_out);

    iov[2].iov_base=(void *) name;
    iov[2].iov_len=len;

    replyVFS:

    alreadywritten+=(* fops->writev)(&conn->io.fuse, iov, 3);

#endif


}

void notify_VFS_create(char *ptr, uint64_t pino, char *name)
{

#if FUSE_VERSION >= 29


#endif

}

void notify_kernel_change(char *ptr, uint64_t ino, uint32_t mask)
{

    /* TODO: */

}

int add_direntry_buffer(void *ptr, struct direntry_buffer_s *buffer, struct name_s *xname, struct stat *st)
{
    size_t dirent_size=offsetof(struct fuse_dirent, name) + xname->len;
    size_t dirent_size_alligned=(((dirent_size) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));

    if (dirent_size_alligned <= buffer->left) {
	struct fuse_dirent *dirent=(struct fuse_dirent *) buffer->pos;

	memset(buffer->pos, 0, dirent_size_alligned); /* to be sure, the buffer should be zerod already */
	dirent->ino=st->st_ino;
	dirent->off=buffer->offset;
	dirent->namelen=xname->len;
	dirent->type=(st->st_mode>0) ? (st->st_mode & S_IFMT) >> 12 : DT_UNKNOWN;
	memcpy(dirent->name, xname->name, xname->len);
	buffer->pos += dirent_size_alligned;
	buffer->left-= dirent_size_alligned;
	buffer->offset++;
	return 0;

    }

    return -1;

}

int add_direntry_plus_buffer(void *ptr, struct direntry_buffer_s *buffer, struct name_s *xname, struct stat *st)
{
    size_t dirent_size=offsetof(struct fuse_direntplus, dirent.name) + xname->len;
    size_t dirent_size_alligned=(((dirent_size) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));

    if (dirent_size_alligned <= buffer->left) {
	struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
	struct fuse_direntplus *direntplus=(struct fuse_direntplus *) buffer->pos;
	struct timespec *attr_timeout=&fusesocket->attr_timeout;
	struct timespec *entry_timeout=&fusesocket->entry_timeout;

	memset(buffer->pos, 0, dirent_size_alligned); /* to be sure, the buffer should be zerod already */

	direntplus->dirent.ino=st->st_ino;
	direntplus->dirent.off=buffer->offset;
	direntplus->dirent.namelen=xname->len;
	direntplus->dirent.type=(st->st_mode>0) ? (st->st_mode & S_IFMT) >> 12 : DT_UNKNOWN;
	memcpy(direntplus->dirent.name, xname->name, xname->len);

	direntplus->entry_out.nodeid=st->st_ino;
	direntplus->entry_out.generation=0; /* ???? */

	direntplus->entry_out.entry_valid=entry_timeout->tv_sec;
	direntplus->entry_out.entry_valid_nsec=entry_timeout->tv_nsec;
	direntplus->entry_out.attr_valid=attr_timeout->tv_sec;
	direntplus->entry_out.attr_valid_nsec=attr_timeout->tv_nsec;

	direntplus->entry_out.attr.ino=st->st_ino;
	direntplus->entry_out.attr.size=st->st_size;
	direntplus->entry_out.attr.blksize=_DEFAULT_BLOCKSIZE;
	direntplus->entry_out.attr.blocks=st->st_size / _DEFAULT_BLOCKSIZE + (st->st_size % _DEFAULT_BLOCKSIZE == 0) ? 0 : 1;

	direntplus->entry_out.attr.atime=(uint64_t) st->st_atim.tv_sec;
	direntplus->entry_out.attr.atimensec=(uint64_t) st->st_atim.tv_nsec;
	direntplus->entry_out.attr.mtime=(uint64_t) st->st_mtim.tv_sec;
	direntplus->entry_out.attr.mtimensec=(uint64_t) st->st_mtim.tv_nsec;
	direntplus->entry_out.attr.ctime=(uint64_t) st->st_ctim.tv_sec;
	direntplus->entry_out.attr.ctimensec=(uint64_t) st->st_ctim.tv_nsec;

	direntplus->entry_out.attr.mode=st->st_mode;
	direntplus->entry_out.attr.nlink=st->st_nlink;
	direntplus->entry_out.attr.uid=st->st_uid;
	direntplus->entry_out.attr.gid=st->st_gid;
	direntplus->entry_out.attr.rdev=0;

	buffer->pos += dirent_size_alligned;
	buffer->left-= dirent_size_alligned;
	buffer->offset++;
	return 0;

    }

    return -1;

}

void reply_VFS_data(struct fuse_request_s *request, char *buffer, size_t size)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) request->ptr;
    struct fs_connection_s *conn=&fusesocket->connection;
    struct fuse_ops_s *fops=conn->io.fuse.fops;
    ssize_t alreadywritten=0;
    struct iovec iov[2];
    struct fuse_out_header oh;

    oh.len=sizeof(struct fuse_out_header) + size;
    oh.error=0;
    oh.unique=request->unique;

    iov[0].iov_base=&oh;
    iov[0].iov_len=sizeof(struct fuse_out_header);

    iov[1].iov_base=buffer;
    iov[1].iov_len=size;

    replyVFS:

    alreadywritten+=(* fops->writev)(&conn->io.fuse, iov, 2);
}

void reply_VFS_error(struct fuse_request_s *request, unsigned int error)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) request->ptr;
    struct fs_connection_s *conn=&fusesocket->connection;
    struct fuse_ops_s *fops=conn->io.fuse.fops;
    ssize_t alreadywritten=0;
    struct fuse_out_header oh;
    struct iovec iov[1];

    oh.len=sizeof(struct fuse_out_header);
    oh.error=-error;
    oh.unique=request->unique;

    iov[0].iov_base=&oh;
    iov[0].iov_len=oh.len;

    replyVFS:

    alreadywritten+=(* fops->writev)(&conn->io.fuse, iov, 1);
}

void reply_VFS_nosys(struct fuse_request_s *request)
{
    reply_VFS_error(request, ENOSYS);
}

void reply_VFS_xattr(struct fuse_request_s *request, size_t size)
{
    struct fuse_getxattr_out getxattr_out;

    getxattr_out.size=size;
    getxattr_out.padding=0;

    reply_VFS_data(request, (char *) &getxattr_out, sizeof(getxattr_out));
}

static void do_reply_nosys(struct fuse_request_s *request)
{
    reply_VFS_nosys(request);
}

static void do_noreply(struct fuse_request_s *request)
{
}

static void do_init(struct fuse_request_s *request)
{
    struct fuse_init_in *init_in=(struct fuse_init_in *) request->buffer;

    if (init_in->major<7) {

	logoutput("do_init: unsupported kernel protocol version");
	reply_VFS_error(request, EPROTO);
	return;

    } else {
	struct fuse_init_out init_out;

	memset(&init_out, 0, sizeof(struct fuse_init_out));

	init_out.major=FUSE_KERNEL_VERSION;
	init_out.minor=FUSE_KERNEL_MINOR_VERSION;
	init_out.flags=0;

	if (init_in->major>7) {

	    reply_VFS_data(request, (char *) &init_out, sizeof(init_out));
	    return;

	} else {

	    init_out.max_readahead = init_in->max_readahead;
	    init_out.max_write = 4096; /* 4K */
	    init_out.max_background=(1 << 16) - 1;
	    init_out.congestion_threshold=(3 * init_out.max_background) / 4;
	    reply_VFS_data(request, (char *) &init_out, sizeof(init_out));

	}

    }

}

void register_fuse_function(char *ptr, uint32_t opcode, void (* cb) (struct fuse_request_s *r))
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;

    if (opcode>0 && opcode<fusesocket->size_cb) {

	fusesocket->fuse_cb[opcode]=cb;

    } else {

	logoutput("register_fuse_function: error opcode %i out of range", opcode);

    }

}

static void _write_datahash(struct list_element_s *list, uint64_t unique, void (* cb)(struct list_header_s *h, struct list_element_s *l))
{
    unsigned int hash=unique % FUSE_HASHTABLE_SIZE;
    struct list_header_s *header=&hashtable[hash];

    write_lock_list_header(header, &hashmutex, &hashcond);
    (* cb)(header, list);
    write_unlock_list_header(header, &hashmutex, &hashcond);

}

static void _add_datahash(struct list_element_s *list, uint64_t unique)
{
    _write_datahash(list, unique, add_list_element_last);
}

static void _remove_list_element(struct list_header_s *h, struct list_element_s *list)
{
    remove_list_element(list);
}

static void _remove_datahash(struct list_element_s *list, uint64_t unique)
{
    _write_datahash(list, unique, _remove_list_element);
}

static unsigned char signal_request_common(struct fusesocket_s *fusesocket, uint64_t unique, unsigned int flag, unsigned int error)
{
    unsigned int hash=unique % FUSE_HASHTABLE_SIZE;
    struct list_element_s *list=NULL;
    struct fuse_request_s *request=NULL;
    unsigned char signal=0;

    pthread_mutex_lock(&hashmutex);
    list=get_list_head(&hashtable[hash], 0);

    while (list) {

	request=(struct fuse_request_s *)((char *) list - offsetof(struct fuse_request_s, list));

	if (request->ptr==(char *) fusesocket && request->unique==unique) {

	    logoutput("signal_request_common: request %lli found", unique);

	    pthread_mutex_lock(&fusesocket->signal.mutex);
	    request->flags|=flag;
	    request->error=error;
	    (* request->set_request_flags)(request);
	    pthread_cond_broadcast(&fusesocket->signal.cond);
	    pthread_mutex_unlock(&fusesocket->signal.mutex);
	    signal=1;
	    break;

	}

	list=get_next_element(list);

    }

    pthread_mutex_unlock(&hashmutex);
    return signal;

}

unsigned char signal_fuse_request_interrupted(char *ptr, uint64_t unique)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return signal_request_common(fusesocket, unique, FUSE_REQUEST_FLAG_INTERRUPTED, EINTR);
}

unsigned char signal_fuse_request_response(char *ptr, uint64_t unique)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return signal_request_common(fusesocket, unique, FUSE_REQUEST_FLAG_RESPONSE, 0);
}

unsigned char signal_fuse_request_error(char *ptr, uint64_t unique, unsigned int error)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return signal_request_common(fusesocket, unique, FUSE_REQUEST_FLAG_ERROR, error);
}

static void signal_fusesocket_common(struct fusesocket_s *fusesocket, unsigned char flag)
{
    if (fusesocket==NULL) return;
    pthread_mutex_lock(&fusesocket->signal.mutex);
    fusesocket->flags |= flag;
    pthread_cond_broadcast(&fusesocket->signal.cond);
    pthread_mutex_unlock(&fusesocket->signal.mutex);
}

static void close_fusesocket(struct fusesocket_s *fusesocket)
{
    pthread_mutex_lock(&fusesocket->mutex);

    if ((fusesocket->flags & FUSE_FLAG_DISCONNECT)==0) {
	struct fs_connection_s *conn=&fusesocket->connection;
	struct fuse_ops_s *fops=conn->io.fuse.fops;

	(* fops->close)(&conn->io.fuse);

	fusesocket->flags |= FUSE_FLAG_DISCONNECT;
	pthread_cond_broadcast(&fusesocket->cond);

    }

    pthread_mutex_unlock(&fusesocket->mutex);

}

static void set_fuse_request_flags_default(struct fuse_request_s *request)
{}

void set_fuse_request_flags_cb(struct fuse_request_s *request, void (* cb)(struct fuse_request_s *request))
{
    pthread_mutex_lock(&hashmutex);
    request->set_request_flags=cb;
    pthread_mutex_unlock(&hashmutex);
}

void set_fuse_request_interrupted(struct fuse_request_s *request, uint64_t ino)
{
    signal_fuse_request_interrupted(request->ptr, ino);
}

static void start_read_fusesocket_buffer(struct fusesocket_s *fusesocket);

/* thread function to process the fuse event stored in fusesocket->buffer */
static void read_fusesocket_buffer(void *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    struct fuse_request_s *request=NULL;
    struct fuse_in_header *in = (struct fuse_in_header *) fusesocket->buffer;
    size_t packetsize=0;

    // logoutput("read_fusesocket_buffer");

    pthread_mutex_lock(&fusesocket->mutex);

    if (fusesocket->read==0 || (fusesocket->flags & FUSE_FLAG_WAIT) || fusesocket->threads>1) {

	logoutput("read_fusesocket_buffer: return");
	pthread_mutex_unlock(&fusesocket->mutex);
	return;

    }

    fusesocket->flags |= (fusesocket->read < sizeof(struct fuse_in_header)) ? FUSE_FLAG_WAITING1 : 0;
    fusesocket->threads++;

    /* make sure the whole header can be read and the buffer is ready to read */

    while (fusesocket->flags & (FUSE_FLAG_WAITING1 | FUSE_FLAG_RECEIVE)) {

	logoutput_debug("read_fusesocket_buffer: wait1 read %i", fusesocket->read);

	int result=pthread_cond_wait(&fusesocket->cond, &fusesocket->mutex);

	if ((fusesocket->read >= sizeof(struct fuse_in_header)) && (fusesocket->flags & FUSE_FLAG_WAITING1)) {

	    fusesocket->flags -= FUSE_FLAG_WAITING1;

	} else if (fusesocket->read==0) {

	    if (fusesocket->flags & FUSE_FLAG_WAITING1) fusesocket->flags -= FUSE_FLAG_WAITING1;
	    fusesocket->threads--;
	    pthread_mutex_unlock(&fusesocket->mutex);
	    return;

	} else if (result>0 || fusesocket->flags & FUSE_FLAG_DISCONNECT) {

	    if (fusesocket->flags & FUSE_FLAG_WAITING1) fusesocket->flags -= FUSE_FLAG_WAITING1;
	    fusesocket->threads--;
	    pthread_mutex_unlock(&fusesocket->mutex);
	    return;

	}

    }

    fusesocket->flags |= FUSE_FLAG_RECEIVE;
    packetsize=in->len;
    in->len -= sizeof(struct fuse_in_header);

    while (fusesocket->read < packetsize) {

	logoutput_debug("read_fusesocket_buffer: wait2 read %i packetsize %i", fusesocket->read, packetsize);

	fusesocket->flags |= FUSE_FLAG_WAITING2;
	int result=pthread_cond_wait(&fusesocket->cond, &fusesocket->mutex);

	if (fusesocket->read >= packetsize) {

	    fusesocket->flags -= FUSE_FLAG_WAITING2;
	    break;

	} else if (result>0 || fusesocket->flags & FUSE_FLAG_DISCONNECT) {

	    fusesocket->flags -= (FUSE_FLAG_RECEIVE | FUSE_FLAG_WAITING2);
	    fusesocket->threads--;
	    pthread_mutex_unlock(&fusesocket->mutex);
	    return;

	}

    }

    /* put data read on simple queue at tail */

    request=malloc(sizeof(struct fuse_request_s) + in->len);

    if (request) {

	request->ctx=fusesocket->context.ctx;
	request->ptr=(char *) fusesocket;
	request->data=NULL;
	request->opcode=in->opcode;
	request->flags=0;
	request->set_request_flags=set_fuse_request_flags_default;
	request->unique=in->unique;
	request->ino=in->nodeid;
	request->uid=in->uid;
	request->gid=in->gid;
	request->pid=in->pid;
	init_list_element(&request->list, NULL);
	request->size=in->len;
	memcpy(request->buffer, fusesocket->buffer + sizeof(struct fuse_in_header), in->len);

	fusesocket->read-=packetsize;
	fusesocket->flags -= FUSE_FLAG_RECEIVE;
	fusesocket->threads--;

	if (fusesocket->read>0) {

	    memmove(fusesocket->buffer, (void *) (fusesocket->buffer + packetsize), fusesocket->read);
	    if (fusesocket->threads==0) {

		start_read_fusesocket_buffer((void *) fusesocket);

	    } else {

		pthread_cond_broadcast(&fusesocket->cond);

	    }

	}

	pthread_mutex_unlock(&fusesocket->mutex);

	logoutput("read_fusesocket_buffer: opcode %i", request->opcode);

	_add_datahash(&request->list, request->unique);
	(* fusesocket->fuse_cb[request->opcode])(request);
	_remove_datahash(&request->list, request->unique);
	free(request);

    } else {
	struct fs_connection_s *conn=&fusesocket->connection;
	struct fuse_ops_s *fops=conn->io.fuse.fops;
	ssize_t alreadywritten=0;
	struct fuse_out_header oh;
	struct iovec iov[1];

	oh.len=sizeof(struct fuse_out_header);
	oh.error=-ENOMEM;
	oh.unique=in->unique;

	iov[0].iov_base=&oh;
	iov[0].iov_len=oh.len;

	alreadywritten+=(* fops->writev)(&conn->io.fuse, iov, 1);

	memcpy(request->buffer, fusesocket->buffer + sizeof(struct fuse_in_header), in->len);
	fusesocket->read-=packetsize;
	fusesocket->flags -= FUSE_FLAG_RECEIVE;
	fusesocket->threads--;

	pthread_mutex_unlock(&fusesocket->mutex);

    }

}

static void start_read_fusesocket_buffer(struct fusesocket_s *fusesocket)
{
    unsigned int error=0;
    work_workerthread(NULL, 0, read_fusesocket_buffer, (void *) fusesocket, NULL);
}

void read_fusesocket_event(int fd, void *ptr, struct event_s *event)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    struct fs_connection_s *conn=&fusesocket->connection;
    struct fuse_ops_s *fops=conn->io.fuse.fops;
    int len=0;
    unsigned int error=0;

    if (signal_is_error(event) || signal_is_close(event)) {

	    /* the remote side (==kernel/VFS) disconnected */

    	    logoutput( "read_fuse_event: event causes disconnect");
	    goto disconnect;

    }

    pthread_mutex_lock(&fusesocket->mutex);

    /* read the data coming from VFS */

    readbuffer:

    errno=0;
    len=(* fops->read)(&conn->io.fuse, (void *)(fusesocket->buffer + fusesocket->read), (size_t) (fusesocket->size - fusesocket->read));
    error=errno;

    /* number bytes read should be at least the size of the incoming header */

    if (len<=0) {

	pthread_mutex_unlock(&fusesocket->mutex);

	logoutput("read_fuse_event: len read %i error %i buffer size %i", len, error, fusesocket->size);

	if (len==0 || error==ECONNRESET || error==ENOTCONN || error==EBADF || error==ENOTSOCK) {

	    /* umount/disconnect */
	    goto disconnect;

	} else if (error==EAGAIN || error==EWOULDBLOCK) {

	    return;

	} else if (error==EINTR) {

	    logoutput("read_fuse_event: read interrupted");

	} else {

	    logoutput("read_fuse_event: error %i %s", error, strerror(error));

	}

    } else {

	/* no error */

	// logoutput("read_fuse_event: len %i", len);
	fusesocket->read+=len;

	if (fusesocket->flags & FUSE_FLAG_WAIT) {

	    pthread_cond_broadcast(&fusesocket->cond);

	} else {

	    start_read_fusesocket_buffer(fusesocket);

	}

	pthread_mutex_unlock(&fusesocket->mutex);

    }

    return;

    disconnect:

    (* fusesocket->context.signal_fuse2ctx)(fusesocket, "command:disconnect", NULL);
    close_fusesocket(fusesocket);
    return;

}

static size_t get_fusesocket_buffer_size()
{
    return getpagesize() + 0x1000;
}

static mode_t get_masked_perm_default(mode_t perm, mode_t mask)
{
    return ((perm & (S_IRWXU | S_IRWXG | S_IRWXO)) & ~mask);
}

static mode_t get_masked_perm_ignore(mode_t perm, mode_t mask)
{
    return (perm & (S_IRWXU | S_IRWXG | S_IRWXO));
}

void umount_fuse_interface(struct pathinfo_s *pathinfo)
{

    if (pathinfo->path && pathinfo->len>0) {

	if (umount2(pathinfo->path, MNT_DETACH)==0) {

	    logoutput_info("umount_fuse_interface: umounted %s", pathinfo->path);

	} else {

	    logoutput_info("umount_fuse_interface: error %i (%s) umounting %s", errno, strerror(errno), pathinfo->path);

	}

    }

}

static int signal_ctx2fuse(void **p_ptr, const char *what, struct ctx_option_s *option)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) *p_ptr;

    if (fusesocket==NULL) return -1;

    logoutput("signal_ctx2fuse: %s", what);

    if (strcmp(what, "command:disconnect:")==0) {

	signal_fusesocket_common(fusesocket, FUSE_FLAG_DISCONNECT);

    } else if (strcmp(what, "command:close:")==0) {

	signal_fusesocket_common(fusesocket, FUSE_FLAG_DISCONNECTED);
	close_fusesocket(fusesocket);

    } else if (strcmp(what, "command:free:")==0) {

	if ((fusesocket->flags & FUSE_FLAG_CLEAR)==0) {

	    pthread_mutex_destroy(&fusesocket->mutex);
	    pthread_cond_destroy(&fusesocket->cond);
	    pthread_mutex_destroy(&fusesocket->signal.mutex);
	    pthread_cond_destroy(&fusesocket->signal.cond);
	    if (fusesocket->flags & FUSE_FLAG_ALLOC) {

		free(fusesocket);
		*p_ptr=NULL;

	    }

	    fusesocket->flags |= FUSE_FLAG_CLEAR;

	}

    }

    return 0;

}

static int signal_fuse2ctx(void *ptr, const char *what, struct ctx_option_s *option)
{
    return 0;
}

void *create_fuse_interface()
{
    struct fusesocket_s *fusesocket=NULL;
    size_t size=get_fusesocket_buffer_size();

    fusesocket=malloc(sizeof(struct fusesocket_s) + size);

    if (fusesocket) {

	memset(fusesocket, 0, sizeof(struct fusesocket_s) + size);
	fusesocket->flags = FUSE_FLAG_ALLOC;
	fusesocket->size=size;

    }

    return (void *) fusesocket;

}

void init_fusesocket(char *ptr, void *ctx, size_t size, unsigned char flags)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;

    logoutput("init_fusesocket");

    fusesocket->flags=0;

    fusesocket->context.ctx=ctx;
    fusesocket->context.signal_fuse2ctx=signal_fuse2ctx;
    fusesocket->context.signal_ctx2fuse=signal_ctx2fuse;

    /* change this .... make this longer for a network fs like 4/5/6 seconds */

    fusesocket->attr_timeout.tv_sec=1;
    fusesocket->attr_timeout.tv_nsec=0;

    fusesocket->entry_timeout.tv_sec=1;
    fusesocket->entry_timeout.tv_nsec=0;

    fusesocket->negative_timeout.tv_sec=1;
    fusesocket->negative_timeout.tv_nsec=0;

    // logoutput("init_fusesocket: A");

    init_connection(&fusesocket->connection, FS_CONNECTION_TYPE_FUSE, FS_CONNECTION_ROLE_CLIENT);

    fusesocket->size_cb=(sizeof(fusesocket->fuse_cb) / sizeof(fusesocket->fuse_cb[0]));
    for (unsigned int i=0; i < fusesocket->size_cb; i++) fusesocket->fuse_cb[i]=do_reply_nosys;
    fusesocket->fuse_cb[FUSE_INIT]=do_init;
    fusesocket->fuse_cb[FUSE_DESTROY]=do_noreply;
    fusesocket->fuse_cb[FUSE_FORGET]=do_noreply;
    fusesocket->fuse_cb[FUSE_BATCH_FORGET]=do_noreply;

    // logoutput("init_fusesocket: B");

    fusesocket->get_masked_perm=get_masked_perm_default;

    pthread_mutex_init(&fusesocket->signal.mutex, NULL);
    pthread_cond_init(&fusesocket->signal.cond, NULL);

    // logoutput("init_fusesocket: C");

    fusesocket->read=0;
    fusesocket->threads=0;
    if (flags & FUSESOCKET_INIT_FLAG_SIZE_INCLUDES_SOCKET) size-=sizeof(struct fusesocket_s);
    fusesocket->size=size;

    pthread_mutex_init(&fusesocket->mutex, NULL);
    pthread_cond_init(&fusesocket->cond, NULL);

    // logoutput("init_fusesocket: D");

}

void disable_masking_userspace(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    if (fusesocket) fusesocket->get_masked_perm=get_masked_perm_ignore;
}

mode_t get_masked_permissions(char *ptr, mode_t perm, mode_t mask)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return (* fusesocket->get_masked_perm)(perm, mask);
}

pthread_mutex_t *get_fuse_pthread_mutex(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return &fusesocket->signal.mutex;
}

pthread_cond_t *get_fuse_pthread_cond(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return &fusesocket->signal.cond;
}

struct timespec *get_fuse_attr_timeout(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return &fusesocket->attr_timeout;
}

struct timespec *get_fuse_entry_timeout(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return &fusesocket->entry_timeout;
}

struct timespec *get_fuse_negative_timeout(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return &fusesocket->negative_timeout;
}

static mode_t get_default_fuse_rootmode()
{
    return (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

static unsigned int get_default_fuse_maxread()
{
    return 8192;
}

struct fs_connection_s *get_fs_connection_fuse(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return &fusesocket->connection;
}

signal_ctx2fuse_t get_signal_ctx2fuse(char *ptr)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    return (fusesocket->context.signal_ctx2fuse);
}

void set_signal_fuse2ctx(char *ptr, signal_fuse2ctx_t cb)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    fusesocket->context.signal_fuse2ctx=cb;
}

static int print_format_mountoptions(struct fusesocket_s *fusesocket, int fd, mode_t mode, uid_t uid, gid_t gid, unsigned int maxread, char *mountoptions, unsigned int len)
{

    if (mountoptions==NULL) {
	int size=128;

	while (1) {
	    char buffer[size];
	    int tmp = snprintf(buffer, size, "fd=%i,rootmode=%o,user_id=%i,group_id=%i,default_permissions,max_read=%i", fd, mode, uid, gid, maxread);

	    if (tmp < size) return tmp+1;
	    size+=128;

	}

    }

    return snprintf(mountoptions, len, "fd=%i,rootmode=%o,user_id=%i,group_id=%i,default_permissions,max_read=%i", fd, mode, uid, gid, maxread);
}

/* connect the fuse interface with the target: the VFS/kernel */

int connect_fusesocket(char *ptr, uid_t uid, char *source, char *mountpoint, char *name, unsigned int *error)
{
    struct fusesocket_s *fusesocket=(struct fusesocket_s *) ptr;
    struct fuse_ops_s *fops=fusesocket->connection.io.fuse.fops;
    int fd=-1;
    struct passwd *pwd=NULL;
    struct ctx_option_s option;
    unsigned int maxread=0;
    mode_t rootmode=0;
    gid_t gid=0;
    int len=0;

    pwd=getpwuid(uid);
    if (pwd==NULL) goto error;
    gid=pwd->pw_gid;

    memset(&option, 0, sizeof(struct ctx_option_s));
    option.type=_CTX_OPTION_TYPE_INT;
    if ((* fusesocket->context.signal_fuse2ctx)(fusesocket, "fuse:maxread", &option)>=0) maxread=(unsigned int) option.value.integer;
    if (maxread==0) maxread=get_default_fuse_maxread();

    memset(&option, 0, sizeof(struct ctx_option_s));
    option.type=_CTX_OPTION_TYPE_INT;
    if ((* fusesocket->context.signal_fuse2ctx)(fusesocket, "fuse:rootmode", &option)>=0) rootmode=(unsigned int) option.value.integer;
    if (rootmode==0) rootmode=get_default_fuse_rootmode();

    fusesocket->flags |= FUSE_FLAG_CONNECTING;
    *error=0;
    fd=(* fops->open)("/dev/fuse", O_RDWR | O_NONBLOCK);

    if (fd < 0) {

	/* unable to open the device */

	logoutput("connect_fusesocket: unable to open /dev/fuse, error %i:%s", errno, strerror(errno));
	fusesocket->flags |= FUSE_FLAG_ERROR;
	*error=errno;
	goto error;

    } else {

	logoutput("connect_fusesocket: fuse device /dev/fuse open with %i", fd);

    }

    if ((len=print_format_mountoptions(fusesocket, fd, rootmode, uid, gid, maxread, NULL, 0))>0) {
	char mountoptions[len];
	unsigned int mountflags=MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_NOATIME;

	len=print_format_mountoptions(fusesocket, fd, rootmode, uid, gid, maxread, mountoptions, len);
	errno=0;

	if (mount(source, mountpoint, "fuse.network", mountflags, (const void *) mountoptions)==0) {

	    logoutput("connect_fusesocket: (fd=%i) mounted %s, type fuse with options %s", fd, mountpoint, mountoptions);
	    fusesocket->flags |= FUSE_FLAG_CONNECTED;

	} else {

	    logoutput("connect_fusesocket: error %i:%s mounting %s with options %s", errno, strerror(errno), mountpoint, mountoptions);
	    fusesocket->flags |= FUSE_FLAG_ERROR;
	    *error=errno;
	    goto error;

	}

    }

    out:
    return fd;

    error:

    if (fd>0) (* fops->close)(&fusesocket->connection.io.fuse);
    return -1;

}

unsigned int get_fuse_buffer_size()
{
    return (sizeof(struct fusesocket_s) + get_fusesocket_buffer_size());
}

void init_hashtable_fusesocket()
{

    pthread_mutex_lock(&hashmutex);

    if (hashinit==0) {

	for (unsigned int i=0; i<FUSE_HASHTABLE_SIZE; i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
	hashinit=1;

    }

    pthread_mutex_unlock(&hashmutex);

}

/*

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

#include <sys/select.h>

#include "libosns-log.h"
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-socket.h"

#include "event.h"

static unsigned int get_msg_header_size_dummy(struct osns_socket_s *sock, void *ptr)
{
    return 0;
}

static unsigned int get_msg_size_dummy(struct osns_socket_s *sock, char *buffer, unsigned int size, void *ptr)
{
    return 0;
}

static void set_msg_size_dummy(struct osns_socket_s *sock, char *buffer, unsigned int size, void *ptr)
{}

static void process_socket_data_default(struct osns_socket_s *sock, char *header, char *buffer, struct socket_control_data_s *ctrl, void *ptr)
{}

void process_socket_close_default(struct osns_socket_s *sock, unsigned int level, void *ptr)
{

    logoutput_debug("process_socket_close_default: fd %i level %u", (* sock->get_unix_fd)(sock), level);

    remove_osns_socket_eventloop(sock);

    if (signal_set_flag(sock->signal, &sock->status, SOCKET_STATUS_CLOSING)) {

	(* sock->close)(sock);
	signal_set_flag(sock->signal, &sock->status, SOCKET_STATUS_CLOSED);

    }

}

void process_socket_error_default(struct osns_socket_s *sock, unsigned int level, unsigned int errcode, void *ptr)
{
    int fd=(* sock->get_unix_fd)(sock);

    /* default: log only
        find out what kind of error ... local like memory allocation or network problems */

    if (fd==-1) {

        /* ignore obvious error's like 9 (Bad file descriptor) after socket has been closed */
        if (socket_connection_error(errcode)) return;

    }

    logoutput_debug("process_socket_error_default: fd %i level %u errcode %u (%s)", fd, level, errcode, strerror(errcode));

}


static unsigned int determine_msg_size(struct osns_socket_s *sock, unsigned int headersize, struct socket_rawdata_s *rawdata, char *buffer, unsigned int *p_bytesread, void *ptr)
{

    if (rawdata->pos < headersize) {
        char tmp[headersize];
        unsigned int bytes2copy=headersize - rawdata->pos;

        if (rawdata->buffer) memcpy(tmp, rawdata->buffer, rawdata->pos);
        memcpy(&tmp[rawdata->pos], buffer, bytes2copy);

//        memmove(buffer, &buffer[bytes2copy], *p_bytesread - bytes2copy);
//        *p_bytesread -= bytes2copy;
//        rawdata->pos=headersize;

        return (* sock->ctx.get_msg_size)(sock, tmp, headersize, ptr);

    }

    return (* sock->ctx.get_msg_size)(sock, rawdata->buffer, rawdata->pos, ptr);

}

uint64_t cb_copy_socket_default(struct osns_socket_s *sock, char *buffer, void *pmsg, unsigned int bytesread, void *ptr)
{
    struct read_socket_data_s *rd=&sock->rd;
    struct list_header_s *h=&rd->in;
    struct list_element_s *list=NULL;
    struct socket_rawdata_s *rawdata=NULL;
    uint64_t ctr=0;
    unsigned int headersize=(* sock->ctx.get_msg_header_size)(sock, ptr);

    signal_lock(sock->signal);
    rd->ctr++;
    ctr=rd->ctr;
    signal_unlock(sock->signal);

    write_lock_list_header(h);

    list=get_list_head(h);

    searchrawdata:

    if (list) {

        rawdata=(struct socket_rawdata_s *)((char *)list - offsetof(struct socket_rawdata_s, list));

        if ((rawdata->flags & SOCKET_RAWDATA_FLAG_FINISH)==0) goto searchunlock;
        rawdata=NULL;
        list=get_next_element(list);
        goto searchrawdata;

    }

    if (rawdata==NULL) {

        rawdata=malloc(sizeof(struct socket_rawdata_s));
        if (rawdata==NULL) goto errormem;
        memset(rawdata, 0, sizeof(struct socket_rawdata_s));
        rawdata->ctr=ctr;
        list=&rawdata->list;
        add_list_element_last(h, list);

    }

    searchunlock:

    write_unlock_list_header(h);

    (* sock->rd.cb_read_cmsg)(sock, pmsg, rawdata);

    if ((rawdata->pos + bytesread) < headersize) {

        if (rawdata->buffer==NULL) {

            rawdata->buffer=malloc(headersize);
            if (rawdata->buffer==NULL) goto errormem;
            rawdata->size=headersize;

        }

        memmove((char *)(rawdata->buffer + rawdata->pos), buffer, bytesread);
        rawdata->pos+=bytesread;

    } else {
        unsigned int msgsize=determine_msg_size(sock, headersize, rawdata, buffer, &bytesread, ptr); /* there is enough data available (at least headersize) to determine the msg size */
        unsigned int bytes2move=((rawdata->pos + bytesread) >= msgsize) ? (msgsize - rawdata->pos) : (bytesread);
        unsigned int bufferpos=0;

        if (rawdata->size < (rawdata->pos + bytes2move)) {

            rawdata->buffer=realloc(rawdata->buffer, (rawdata->pos + bytes2move));
            if (rawdata->buffer==NULL) goto errormem;
            rawdata->size=rawdata->pos + bytes2move;

        }

        memmove((char *)(rawdata->buffer + rawdata->pos), buffer, bytes2move);
        rawdata->pos+=bytes2move;

        if (rawdata->pos==msgsize) {

            /* ready: signal */
            signal_set_flag(sock->signal, &rawdata->flags, SOCKET_RAWDATA_FLAG_FINISH);

        }

        bufferpos=bytes2move;
        bytesread-=bytes2move;

        while (bytesread>0) {

            rawdata=malloc(sizeof(struct socket_rawdata_s));
            if (rawdata==NULL) goto errormem;
            memset(rawdata, 0, sizeof(struct socket_rawdata_s));
            rawdata->ctr=ctr;
            write_lock_list_header(h);
            add_list_element_last(h, &rawdata->list);
            write_unlock_list_header(h);

            if (bytesread < headersize) {

                rawdata->buffer=malloc(headersize);
                if (rawdata->buffer==NULL) goto errormem;
                rawdata->size=headersize;
                memmove(rawdata->buffer, &buffer[bufferpos], bytesread);
                rawdata->pos=bytesread;
                break; /* ready ... msg not complete */

            } else {
                unsigned int msgsize=(* sock->ctx.get_msg_size)(sock, &buffer[bufferpos], bytesread, ptr);

                if (bytesread < msgsize) {

                    rawdata->buffer=malloc(bytesread);
                    if (rawdata->buffer==NULL) goto errormem;
                    rawdata->size=bytesread;
                    memmove(rawdata->buffer, &buffer[bufferpos], bytesread);
                    rawdata->pos=bytesread;
                    break; /* ready ... msg not complete */

                }

                rawdata->buffer=malloc(msgsize);
                if (rawdata->buffer==NULL) goto errormem;
                rawdata->size=msgsize;
                memmove(rawdata->buffer, &buffer[bufferpos], msgsize);
                rawdata->pos=rawdata->size;
                bytesread -= msgsize;
                bufferpos += msgsize;
                /* msg complete ... still not ready when bytes are left */
                signal_set_flag(sock->signal, &rawdata->flags, SOCKET_RAWDATA_FLAG_FINISH);

            }

        }

    }

    return ctr;

    errormem:
    logoutput_debug("cb_copy_socket_default: unable to allocate memory ... fatal");
    return ctr;

}

static void process_data_buffer(struct osns_socket_s *sock, char *buffer, unsigned int size, struct socket_control_data_s *ctrl, void *ptr)
{
    struct read_socket_data_s *rd=&sock->rd;
    unsigned int headersize=(* sock->ctx.get_msg_header_size)(sock, ptr);
    char header[headersize];

    memcpy(header, buffer, headersize);
    size -= headersize;
    memmove(buffer, (char *)(buffer + headersize), size);
    (* sock->ctx.set_msg_size)(sock, header, size, ptr);
    (* sock->ctx.process_data)(sock, header, buffer, ctrl, ptr);
    free(buffer);
}

struct socket_rawdata_s *get_socket_rawdata_from_list(struct osns_socket_s *sock, uint64_t ctr)
{
    struct list_header_s *h=&sock->rd.in;
    struct socket_rawdata_s *rawdata=NULL;
    struct list_element_s *list=NULL;

    write_lock_list_header(h);
    list=get_list_head(h);

    while (list) {

        rawdata=(struct socket_rawdata_s *)((char *)list - offsetof(struct socket_rawdata_s, list));

        if (rawdata->ctr > ctr) {

            rawdata=NULL;
            break;

        } else if (signal_set_flag(sock->signal, &rawdata->flags, SOCKET_RAWDATA_FLAG_WATCHED)) {

            break;

        }

        list=get_next_element(list);
        rawdata=NULL;

    }

    write_unlock_list_header(h);
    return rawdata;
}

static void cb_read_socket_default(struct osns_socket_s *sock, uint64_t ctr, void *ptr)
{
    struct socket_rawdata_s *rawdata=NULL;

    processlist:

    rawdata=get_socket_rawdata_from_list(sock, ctr);

    if (rawdata) {
        struct shared_signal_s *signal=sock->signal;

        signal_lock(signal);

        while (((rawdata->flags & SOCKET_RAWDATA_FLAG_FINISH)==0) && ((sock->status & (SOCKET_STATUS_ERROR | SOCKET_STATUS_CLOSING))==0)) {

            int result=signal_condwait(signal);

        }

        signal_unlock(signal);

        if (rawdata->flags & SOCKET_RAWDATA_FLAG_FINISH) {
            struct list_header_s *h=&sock->rd.in;
            struct list_element_s *list=&rawdata->list;
            char *buffer=rawdata->buffer;
            unsigned int size=rawdata->size;
            struct socket_control_data_s ctrl;

            write_lock_list_header(h);
            remove_list_element(list);
            write_unlock_list_header(h);

            memcpy(&ctrl, &rawdata->ctrl, sizeof(struct socket_control_data_s));
            free(rawdata);

            if (buffer) process_data_buffer(sock, buffer, size, &ctrl, ptr);
            goto processlist;

        }

    }

}

static void read_osns_socket_cmsg_default(struct osns_socket_s *sock, void *pmsg, struct socket_rawdata_s *rawdata)
{
    struct read_socket_data_s *rd=&sock->rd;

    if ((rd->cbuffer) && ((rawdata->flags & SOCKET_RAWDATA_FLAG_CMSG)==0) && pmsg) {
        struct msghdr *msg=(struct msghdr *) pmsg;
        struct cmsghdr *cmsg=CMSG_FIRSTHDR(msg);

        /* in case of there is additional data (like fd) copy that */

        if (cmsg) {

	    if (cmsg->cmsg_level==SOL_SOCKET && cmsg->cmsg_type==SCM_RIGHTS) {

	        rawdata->ctrl.type=SOCKET_CONTROL_DATA_TYPE_FD;
	        memcpy(&rawdata->ctrl.data.fd, CMSG_DATA(cmsg), sizeof(int));

	    }

        }

        rawdata->flags |= SOCKET_RAWDATA_FLAG_CMSG;

    }

}

static void read_osns_socket_cmsg_ignore(struct osns_socket_s *sock, void *pmsg, struct socket_rawdata_s *rawdata)
{
}


void set_socket_context_defaults(struct osns_socket_s *sock)
{

    sock->ctx.get_msg_header_size=get_msg_header_size_dummy;
    sock->ctx.get_msg_size=get_msg_size_dummy;
    sock->ctx.set_msg_size=set_msg_size_dummy;

    sock->ctx.process_data=process_socket_data_default;
    sock->ctx.process_close=process_socket_close_default;
    sock->ctx.process_error=process_socket_error_default;

    sock->ctx.copy=cb_copy_socket_default;
    sock->ctx.read=cb_read_socket_default;

    sock->ctx.ptr=NULL;

    sock->rd.cb_read_cmsg=read_osns_socket_cmsg_ignore;
}

/* functions which link bevent/eventloop to socket */

static void process_connection_socket_msg_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    void *ptr=bevent->ptr;
    struct read_socket_data_s *rd=&sock->rd;
    int bytesread=0;
    unsigned int errcode=0;
    struct iovec iov[1];
    struct msghdr msg;

    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;
    msg.msg_flags=0;

#ifdef __linux__

    msg.msg_control=rd->cbuffer; /* may be NULL */
    msg.msg_controllen=rd->csize;

#else

    msg.msg_control=NULL;
    msg.msg_controllen=0;

#endif

    iov[0].iov_base=(void *) (rd->buffer + rd->pos);
    iov[0].iov_len=(rd->size - rd->pos);

    clear_io_event(bevent, flag, arg);
    signal_lock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
    bytesread=(* sock->sops.connection.recvmsg)(sock, &msg);
    errcode=errno;

    if (bytesread>0) {
        uint64_t ctr=0;

        logoutput_debug("process_connection_socket_msg_event: fd %i buffer size %u bytesread %i cbuffer size %u", (* sock->get_unix_fd)(sock), rd->size, bytesread, msg.msg_controllen);

        ctr=(* sock->ctx.copy)(sock, rd->buffer, (void *) &msg, (unsigned int) bytesread, ptr);
        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.read)(sock, ctr, ptr);
        return;

    } else if (bytesread==0) {

        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.process_close)(sock, SOCKET_LEVEL_REMOTE, ptr);
        return;

    }

    signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
    if (errcode==EAGAIN) return;
    (* sock->ctx.process_error)(sock, SOCKET_LEVEL_NETWORK, errcode, ptr);

}

static void process_connection_socket_recv_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    void *ptr=bevent->ptr;
    struct read_socket_data_s *rd=&sock->rd;
    int bytesread=0;
    unsigned int errcode=0;

    clear_io_event(bevent, flag, arg);
    signal_lock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);

    readsocket:

    bytesread=(* sock->sops.connection.recv)(sock, (rd->buffer + rd->pos), (rd->size - rd->pos), 0);
    errcode=errno;

    if (bytesread>0) {
        uint64_t ctr=0;

        logoutput_debug("process_connection_socket_recv_event: fd %i buffer size %u bytesread %i", (* sock->get_unix_fd)(sock), rd->size, bytesread);

        ctr=(* sock->ctx.copy)(sock, rd->buffer, NULL, (unsigned int) bytesread, ptr);
        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.read)(sock, ctr, ptr);
        return;

    } else if (bytesread==0) {

        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.process_close)(sock, SOCKET_LEVEL_REMOTE, ptr);
        return;

    } else {

        /* bytesread<0: error */

        if (errcode==EAGAIN) {
            fd_set rset;
            struct timeval timeout;
            int fd=(* sock->get_unix_fd)(sock);

            FD_ZERO(&rset);
            FD_SET(fd, &rset);
            timeout.tv_sec=0;
            timeout.tv_usec=250;

            int result=select(fd+1, &rset, NULL, NULL, &timeout);
            if (result>0) goto readsocket;

        }

        logoutput_debug("process_connection_socket_recv_event: fd %i errcode %u", (* sock->get_unix_fd)(sock), errcode);

    }

    signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
    (* sock->ctx.process_error)(sock, SOCKET_LEVEL_NETWORK, errcode, ptr);

}

static void process_device_socket_data_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    void *ptr=bevent->ptr;
    struct read_socket_data_s *rd=&sock->rd;
    int bytesread=0;
    unsigned int errcode=0;

    clear_io_event(bevent, flag, arg);
    signal_lock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);

    readsocket:

    bytesread=(* sock->sops.device.read)(sock, (rd->buffer + rd->pos), (rd->size - rd->pos));
    errcode=errno;

    if (bytesread>0) {
        uint64_t ctr=0;

        logoutput_debug("process_device_socket_data_event: fd %i buffer size %u bytesread %i", (* sock->get_unix_fd)(sock), rd->size, bytesread);

        ctr=(* sock->ctx.copy)(sock, rd->buffer, NULL, (unsigned int) bytesread, ptr);
        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.read)(sock, ctr, ptr);
        return;

    } else if (bytesread==0) {

        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.process_close)(sock, SOCKET_LEVEL_REMOTE, ptr);
        return;

    }

    if (errcode==EAGAIN) goto readsocket;

    signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
    if (errcode==EAGAIN) return;
    (* sock->ctx.process_error)(sock, SOCKET_LEVEL_NETWORK, errcode, ptr);

}

static void process_device_socket_readv_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    void *ptr=bevent->ptr;
    struct read_socket_data_s *rd=&sock->rd;
    int bytesread=0;
    unsigned int errcode=0;
    struct iovec iov[1];

    iov[0].iov_base=(void *) (rd->buffer + rd->pos);
    iov[0].iov_len=(rd->size - rd->pos);

    clear_io_event(bevent, flag, arg);
    signal_lock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);

    readsocket:

    bytesread=(* sock->sops.device.readv)(sock, iov, 1);
    errcode=errno;

    if (bytesread>0) {
        uint64_t ctr=0;

        logoutput_debug("process_device_socket_readv_event: fd %i buffer size %u bytesread %i", (* sock->get_unix_fd)(sock), rd->size, bytesread);

        ctr=(* sock->ctx.copy)(sock, rd->buffer, NULL, (unsigned int) bytesread, ptr);
        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.read)(sock, ctr, ptr);
        return;

    } else if (bytesread==0) {

        signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
        (* sock->ctx.process_close)(sock, SOCKET_LEVEL_REMOTE, ptr);
        return;

    }

    if (errcode==EAGAIN) goto readsocket;

    signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
    (* sock->ctx.process_error)(sock, SOCKET_LEVEL_NETWORK, errcode, ptr);

}

static void process_socket_custom_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    clear_io_event(bevent, flag, arg);
    (* sock->ctx.process_data)(sock, NULL, NULL, NULL, bevent->ptr);
}

static void process_socket_close_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    void *ptr=bevent->ptr;
    (* sock->ctx.process_close)(sock, SOCKET_LEVEL_NETWORK, ptr);
}

static void process_socket_error_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct osns_socket_s *sock=bevent->sock;
    void *ptr=bevent->ptr;
    clear_io_event(bevent, flag, arg);
    (* sock->ctx.process_error)(sock, SOCKET_LEVEL_NETWORK, ENOTCONN, ptr);
}

static void set_bevent_process_data(struct bevent_s *bevent, unsigned int flags)
{

    if (flags & OSNS_SOCKET_ENABLE_CUSTOM_READ) {

        set_bevent_cb(bevent, BEVENT_FLAG_CB_DATA, process_socket_custom_event);

    } else {
        struct osns_socket_s *sock=bevent->sock;

        if (sock->type==OSNS_SOCKET_TYPE_CONNECTION) {

            if (flags & OSNS_SOCKET_ENABLE_CTRL_DATA) {

                set_bevent_cb(bevent, BEVENT_FLAG_CB_DATA, process_connection_socket_msg_event);

            } else {

                set_bevent_cb(bevent, BEVENT_FLAG_CB_DATA, process_connection_socket_recv_event);

            }

        } else if (sock->type==OSNS_SOCKET_TYPE_DEVICE) {

            set_bevent_cb(bevent, BEVENT_FLAG_CB_DATA, process_device_socket_readv_event);

        }

    }

}

void set_bevent_process_data_custom(struct bevent_s *bevent)
{
    set_bevent_process_data(bevent, OSNS_SOCKET_ENABLE_CUSTOM_READ);
}

void set_bevent_process_data_default(struct bevent_s *bevent)
{
    set_bevent_process_data(bevent, 0);
}

int add_osns_socket_eventloop(struct osns_socket_s *sock, struct beventloop_s *loop, void *ptr, unsigned int flags)
{
    struct bevent_s *bevent=create_fd_bevent(loop, ptr);
    int result=-1;

    if (bevent) {

	set_bevent_osns_socket(bevent, sock);
        if (sock->rd.cbuffer && (sock->rd.csize>0)) flags |= OSNS_SOCKET_ENABLE_CTRL_DATA;
        set_bevent_process_data(bevent, flags);
	set_bevent_cb(bevent, BEVENT_FLAG_CB_CLOSE, process_socket_close_event);
	set_bevent_cb(bevent, BEVENT_FLAG_CB_ERROR, process_socket_error_event);
	add_bevent_watch(bevent);
        result=0;

    }

    return result;

}

void reset_osns_socket_process_data(struct osns_socket_s *sock, unsigned int flags)
{
    struct bevent_s *bevent=sock->event.bevent;
    set_bevent_process_data(bevent, flags);
}

void remove_osns_socket_eventloop(struct osns_socket_s *sock)
{

    if (signal_unset_flag(sock->signal, &sock->flags, OSNS_SOCKET_FLAG_BEVENT)) {
	struct bevent_s *bevent=sock->event.bevent;

	if (bevent) {

            remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);
    	    free_bevent(&bevent);
            sock->event.bevent=NULL;

        }

    }

}

struct beventloop_s *osns_socket_get_eventloop(struct osns_socket_s *sock)
{
    struct beventloop_s *loop=NULL;

    if (sock->flags & OSNS_SOCKET_FLAG_BEVENT) {
	struct bevent_s *bevent=sock->event.bevent;

	if (bevent) loop=get_eventloop_bevent(bevent);

    }

    return loop;
}

void set_osns_socket_buffer(struct osns_socket_s *sock, char *buffer, unsigned int size)
{
    sock->rd.buffer=buffer;
    sock->rd.size=size;
}

void set_osns_socket_control_data_buffer(struct osns_socket_s *sock, char *buffer, unsigned int size)
{
    sock->rd.cbuffer=buffer;
    sock->rd.csize=size;
}

void set_read_osns_socket_cmsg(struct osns_socket_s *sock, unsigned char enable)
{
    //signal_lock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
    sock->rd.cb_read_cmsg=((enable) ? read_osns_socket_cmsg_default : read_osns_socket_cmsg_ignore);
    //signal_unlock_flag(sock->signal, &sock->status, SOCKET_STATUS_READ);
}

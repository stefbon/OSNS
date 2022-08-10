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

#include "osns_sftp_subsystem.h"
#include "ssh/subsystem/connection.h"
#include "libosns-sftp.h"

#include "receive.h"
#include "send.h"
#include "init.h"
#include "supported.h"
#include "payload.h"
#include "protocol.h"
#include "attr.h"
#include "path.h"
#include "extensions.h"

#include "cb-open.h"
#include "cb-close.h"
#include "cb-opendir.h"
#include "cb-stat.h"
#include "cb-rm.h"
#include "cb-mk.h"
#include "cb-readlink.h"

#include "extensions.h"

/*
    for sftp init data looks like:

    - 4 bytes                           len sftp packet excl this field
    - byte                              sftp type
    - 4 bytes                           sftp version server
    - extension-pair                    extension[0..n]

    where one extension has the form:
    - string name
    - string data

    for version 6 the extensions supported are:
    - supported2
    - acl-supported
*/

int send_sftp_init(struct sftp_subsystem_s *sftp)
{
    struct sftp_init_extensions_s attr_extensions={0, NULL, 0};
    struct sftp_init_extensions_s protocol_extensions={0, NULL, 0};
    unsigned int attrib_extensions_count=0;
    unsigned int extensions_count=0;
    unsigned int len_h=4 + strlen("supported2");
    unsigned int len_hs=32 + get_sftp_attrib_extensions(sftp, &attr_extensions) + get_sftp_protocol_extensions(sftp, &protocol_extensions);
    unsigned int len_a=4 + strlen("acl-supported");
    unsigned int len_as=4;
    unsigned int len=9 + len_h + len_hs + len_a + len_as;
    char data[len];
    unsigned int pos=0;
    unsigned int error=0;

    logoutput("send_sftp_init");

    /* SSH_FXP_VERSION message */

    store_uint32(&data[pos], len-4);
    pos+=4;

    data[pos]=SSH_FXP_VERSION;
    pos++;

    store_uint32(&data[pos], get_sftp_protocol_version(sftp));
    pos+=4;

    /* header supported2 */

    pos+=write_ssh_string(&data[pos], len-pos, 'c', "supported2");

    /* supported-structure */

    store_uint32(&data[pos], len_hs);
    pos+=4;

    /* valid attribute flags */

    store_uint32(&data[pos], get_supported_attr_valid_mask(sftp));
    pos+=4;

    /* valid attribute bits */

    store_uint32(&data[pos], get_supported_attr_attr_bits(sftp));
    pos+=4;

    /* supported open flags */

    store_uint32(&data[pos], get_supported_open_flags(sftp));
    pos+=4;

    /* supported access mask */

    store_uint32(&data[pos], get_supported_open_access(sftp));
    pos+=4;

    /* max read size */

    store_uint32(&data[pos], get_supported_max_readsize(sftp));
    pos+=4;

    /* supported open-block-vector */

    store_uint16(&data[pos], get_supported_open_block_flags(sftp));
    pos+=2;

    /* supported block-vector */

    store_uint16(&data[pos], get_supported_block_flags(sftp));
    pos+=2;

    /* attrib extension count */

    store_uint32(&data[pos], attr_extensions.count);
    pos+=4;

    /* attrib names */

    attr_extensions.len=len - pos;
    attr_extensions.buffer=&data[pos];
    attr_extensions.count=0;

    pos+=get_sftp_attrib_extensions(sftp, &attr_extensions);

    /* protocol extension count */

    store_uint32(&data[pos], protocol_extensions.count);
    pos+=4;

    /* protocol extension names */

    protocol_extensions.len=len - pos;
    protocol_extensions.buffer=&data[pos];
    protocol_extensions.count=0;

    pos+=get_sftp_protocol_extensions(sftp, &protocol_extensions);

    /* acl supported */

    pos+=write_ssh_string(&data[pos], len-pos, 'c', "acl-supported");

    /* acl structure */

    store_uint32(&data[pos], len_as);
    pos+=4;

    /* acl capabilities */

    store_uint32(&data[pos], get_supported_acl_cap(sftp));
    pos+=4;

    /* store length in first 4 bytes */

    store_uint32(&data[0], pos-4);

    if (send_sftp_subsystem(sftp, data, pos)>=0) {

        sftp->flags|=SFTP_SUBSYSTEM_FLAG_VERSION_SEND;
        return 0;

    }

    if (error==0) error=EIO;
    logoutput("send_sftp_init: error %i sending init (%s)", error, strerror(error));

    return -1;

}

static void reply_sftp_notsupported(struct sftp_payload_s *p)
{
    int result=reply_sftp_status_simple(p->sftp, p->id, SSH_FX_OP_UNSUPPORTED);
}

int check_sftp_cb_is_taken(struct sftp_subsystem_s *sftp, unsigned char code)
{
    return ((sftp->cb[code]==reply_sftp_notsupported) ? 0 : 1);
}

/* use with care, this can override a default callback
    (there is no check here) */

void set_sftp_cb(struct sftp_subsystem_s *sftp, unsigned char code, void (* cb)(struct sftp_payload_s *p))
{
    sftp->cb[code]=((cb) ? cb : reply_sftp_notsupported);
}

static int init_sftp_identity(struct sftp_identity_s *user)
{
    char *name=NULL;

    memset(user, 0, sizeof(struct sftp_identity_s));
    init_ssh_string(&user->home); /* homedirectory of connecting user (==user under which this subsystem is running)*/

    user->buffer=NULL;
    user->size=128;

    /* assume the process is running as the desired user already, so getting the environment variable LOGNAME is enough */

    name=getenv("LOGNAME");

    if (name) {
	struct passwd *result=NULL;

	logoutput("init_sftp_identity: found name %s", name);

	getpw:

	memset(&user->pwd, 0, sizeof(struct passwd));
	result=NULL;

	user->buffer=realloc(user->buffer, user->size);
	if (user->buffer==NULL) {

	    logoutput_warning("init_sftp_identity: unable to allocate %i bytes ... cannot continue", user->size);
	    return -1;

	}

	if (getpwnam_r(name, &user->pwd, user->buffer, user->size, &result)==-1) {

	    if (errno==ERANGE) {

		user->size+=128;
		goto getpw;

	    }

	    logoutput_warning("init_sftp_identity: unexpected error %i:%s ... cannot continue", errno, strerror(errno));
	    return -1;

	}

	user->home.ptr=user->pwd.pw_dir;
	user->home.len=strlen(user->home.ptr);

    }

    return 0;

}

static void free_sftp_identity(struct sftp_identity_s *user)
{
    if (user->buffer) {

	free(user->buffer);
	user->buffer=NULL;

    }

    user->size=0;
}

static void init_sftp_payload_queue(struct sftp_payload_queue_s *queue)
{
    memset(queue, 0, sizeof(struct sftp_payload_queue_s));
    init_list_header(&queue->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    queue->threads=0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

static void free_sftp_payload_queue(struct sftp_payload_queue_s *queue)
{
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    memset(queue, 0, sizeof(struct sftp_payload_queue_s));
}

unsigned char get_sftp_protocol_version(struct sftp_subsystem_s *sftp)
{
    unsigned char version = (sftp) ? sftp->protocol.version : 0;

    /* TODO ... */

    return (unsigned char) ((version>0) ? version : 6);

}

void set_sftp_protocol_version(struct sftp_subsystem_s *sftp, unsigned char version)
{
    sftp->protocol.version=version;
}

void setup_sftp_idmapping(struct sftp_subsystem_s *sftp)
{
    struct net_idmapping_s *mapping=&sftp->mapping;
    unsigned int flags=NET_IDMAPPING_FLAG_SERVER;
    unsigned int version=get_sftp_protocol_version(sftp);

    if (version > 3) {

	flags |= NET_IDMAPPING_FLAG_MAPBYNAME;

    } else {

	flags |= NET_IDMAPPING_FLAG_MAPBYID;

    }

    set_net_entity_map_func(mapping, flags);

}

void init_sftp_prefix(struct sftp_subsystem_s *sftp)
{
    struct sftp_prefix_s *prefix=&sftp->prefix;

    prefix->flags=0;
    init_ssh_string(&prefix->path);
    prefix->get_length_fullpath=get_length_fullpath_noprefix;

}

int init_sftp_subsystem(struct sftp_subsystem_s *sftp)
{

    init_hashattr_generic();

    memset(sftp, 0, sizeof(struct sftp_subsystem_s));
    sftp->flags = SFTP_SUBSYSTEM_FLAG_INIT;

    init_sftp_prefix(sftp);
    init_sftp_identity(&sftp->identity);
    init_ssh_subsystem_connection(&sftp->connection, 0, sftp->signal, read_ssh_subsystem_connection_socket);
    init_sftp_receive(&sftp->receive);
    init_sftp_payload_queue(&sftp->queue);
    sftp->signal=get_default_shared_signal();

    set_sftp_protocol_version(sftp, 6);
    init_sftp_subsystem_attr_context(sftp);
    init_net_idmapping(&sftp->mapping, &sftp->identity.pwd);
    set_sftp_attr_context(&sftp->attrctx);

    set_process_sftp_payload_notsupp(sftp);
    for (unsigned int i=0; i<256; i++) sftp->cb[i]=reply_sftp_notsupported;

    sftp->cb[SSH_FXP_OPENDIR]=sftp_op_opendir;
    sftp->cb[SSH_FXP_READDIR]=sftp_op_readdir;

    sftp->cb[SSH_FXP_CLOSE]=sftp_op_close;

    sftp->cb[SSH_FXP_OPEN]=sftp_op_open;
    sftp->cb[SSH_FXP_READ]=sftp_op_read;
    sftp->cb[SSH_FXP_WRITE]=sftp_op_write;

    sftp->cb[SSH_FXP_LSTAT]=sftp_op_lstat;
    sftp->cb[SSH_FXP_STAT]=sftp_op_stat;
    sftp->cb[SSH_FXP_FSTAT]=sftp_op_fstat;

    sftp->cb[SSH_FXP_READLINK]=sftp_op_readlink;

    sftp->cb[SSH_FXP_REMOVE]=sftp_op_remove;
    sftp->cb[SSH_FXP_RMDIR]=sftp_op_rmdir;
    sftp->cb[SSH_FXP_MKDIR]=sftp_op_mkdir;

    sftp->cb[SSH_FXP_EXTENDED]=sftp_op_extension;

    return 0;

}

void free_sftp_subsystem(struct sftp_subsystem_s *sftp)
{
    free_net_idmapping(&sftp->mapping);
    free_sftp_identity(&sftp->identity);
    free_sftp_payload_queue(&sftp->queue);
    free_sftp_receive(&sftp->receive);

    clear_ssh_subsystem_connection(&sftp->connection);
    clear_hashattr_generic(0);
}

void finish_sftp_subsystem(struct sftp_subsystem_s *sftp)
{
    logoutput_info("finish_sftp_subsystem");

    /* close the connection...
	remove the fd's from eventloop
	send a disconnect to the context */
}

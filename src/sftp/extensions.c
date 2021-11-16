/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "threads.h"

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-channel.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh-receive.h"

#include "sftp/common-protocol.h"
#include "common.h"
#include "request-hash.h"

#define NAME_MAPEXTENSION_DEFAULT			"mapextension@bononline.nl"
#define SFTP_EXTENSION_NAME_STATVFS			"statvfs@openssh.com"
#define SFTP_EXTENSION_NAME_FSYNC			"fsync@openssh.com"

void init_sftp_extensions(struct sftp_client_s *sftp)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    init_list_header(&extensions->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    extensions->count=0;
    extensions->mapped=SSH_FXP_MAPPING_MIN;
    extensions->mapextension=NULL;
    extensions->fsync=NULL;
    extensions->statvfs=NULL;
}

void clear_sftp_extensions(struct sftp_client_s *sftp)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct list_element_s *list=NULL;

    list=get_list_head(&extensions->header, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct sftp_protocolextension_s *extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	free(extension);
	list=get_list_head(&extensions->header, SIMPLE_LIST_FLAG_REMOVE);

    }

    extensions->count=0;
    extensions->mapextension=NULL;

}

static int handle_sftp_reply(struct sftp_request_s *sftp_r, struct sftp_reply_s *reply, unsigned int *error)
{
    int result=0;

    reply->type=sftp_r->reply.type;

    if (reply->type==SSH_FXP_STATUS) {

	reply->response.status.code=sftp_r->reply.response.status.code;
	reply->response.status.linux_error=sftp_r->reply.response.status.linux_error;
	reply->response.status.buff=sftp_r->reply.response.status.buff;
	reply->response.status.size=sftp_r->reply.response.status.size;
	sftp_r->reply.response.status.buff=NULL;
	sftp_r->reply.response.status.size=0;

    } else if (reply->type==SSH_FXP_HANDLE) {

	reply->response.handle.name=sftp_r->reply.response.handle.name;
	reply->response.handle.len=sftp_r->reply.response.handle.len;
	sftp_r->reply.response.handle.name=NULL;
	sftp_r->reply.response.handle.len=0;

    } else if (reply->type==SSH_FXP_DATA) {

	reply->response.data.data=sftp_r->reply.response.data.data;
	reply->response.data.size=sftp_r->reply.response.data.size;
	reply->response.data.flags=sftp_r->reply.response.data.flags;
	sftp_r->reply.response.data.data=NULL;
	sftp_r->reply.response.data.size=0;

    } else if (reply->type==SSH_FXP_NAME) {

	reply->response.names.count=sftp_r->reply.response.names.count;
	reply->response.names.size=sftp_r->reply.response.names.size;
	reply->response.names.flags=sftp_r->reply.response.names.flags;
	reply->response.names.buff=sftp_r->reply.response.names.buff;
	sftp_r->reply.response.names.buff=NULL;
	sftp_r->reply.response.names.size=0;

    } else if (reply->type==SSH_FXP_ATTRS) {

	reply->response.attr.buff=sftp_r->reply.response.attr.buff;
	reply->response.attr.size=sftp_r->reply.response.attr.size;
	sftp_r->reply.response.attr.buff=NULL;
	sftp_r->reply.response.attr.size=0;

    } else if (reply->type==SSH_FXP_EXTENDED_REPLY) {

	reply->response.extension.buff=sftp_r->reply.response.extension.buff;
	reply->response.extension.size=sftp_r->reply.response.extension.size;
	sftp_r->reply.response.extension.buff=NULL;
	sftp_r->reply.response.extension.size=0;

    } else {

	*error=EPROTO;
	result=-1;

    }

    reply->error=sftp_r->reply.error;

    return result;

}

static int _send_sftp_extension_compat(struct sftp_client_s *sftp, struct sftp_protocolextension_s *extension, struct sftp_request_s *sftp_r, unsigned int *error)
{
    memset(&sftp_r->call.extension.id, 0, sizeof(sftp_r->call.extension.id));
    sftp_r->call.extension.type=SFTP_EXTENSION_TYPE_DEFAULT;
    sftp_r->call.extension.id.name.len=extension->name.len;
    sftp_r->call.extension.id.name.name=(unsigned char *) extension->name.ptr;
    return (* sftp->send_ops->extension)(sftp, sftp_r);
}

static int _send_sftp_extension_default(struct sftp_client_s *sftp, struct sftp_protocolextension_s *extension, struct sftp_request_s *sftp_r, unsigned int *error)
{
    return _send_sftp_extension_compat(sftp, extension, sftp_r, error);
}

static int _send_sftp_extension_custom(struct sftp_client_s *sftp, struct sftp_protocolextension_s *extension, struct sftp_request_s *sftp_r, unsigned int *error)
{
    memset(&sftp_r->call.extension.id, 0, sizeof(sftp_r->call.extension.id));
    sftp_r->call.extension.type=SFTP_EXTENSION_TYPE_CUSTOM;
    sftp_r->call.extension.id.nr=extension->mapped;
    sftp_r->status=SFTP_REQUEST_STATUS_WAITING;
    return (* sftp->send_ops->custom)(sftp, sftp_r);
}

static int _send_sftp_extension_notsupp(struct sftp_client_s *sftp, struct sftp_protocolextension_s *extension, struct sftp_request_s *sftp_r, unsigned int *error)
{
    *error=EOPNOTSUPP;
    return -1;
}

static void dummy_event_cb(struct ssh_string_s *name, struct ssh_string_s *data, void *ptr, unsigned int event)
{
}

static struct sftp_protocolextension_s *_add_sftp_extension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data, unsigned int flags)
{
    struct sftp_protocolextension_s *extension=NULL;

    if (name==NULL || name->len==0 || name->ptr==NULL) return NULL;

    extension=malloc(sizeof(struct sftp_protocolextension_s) + name->len + ((data) ? data->len : 0));

    if (extension) {
	struct sftp_extensions_s *extensions=&sftp->extensions;
	unsigned int pos=0;
	unsigned int len=name->len + (data ? data->len : 0);

	memset(extension, 0, sizeof(struct sftp_protocolextension_s) + len);
	extension->flags=0;
	extension->mapped=0;
	extension->ptr=NULL;
	extension->event_cb=dummy_event_cb;
	extension->send_extension=_send_sftp_extension_notsupp;
	init_list_element(&extension->list, NULL);
	memcpy(&extension->buffer[pos], name->ptr, name->len);
	extension->name.ptr=&extension->buffer[pos];
	extension->name.len=name->len;
	pos+=name->len;

	if (data && data->len>0) {

	    memcpy(&extension->buffer[pos], data->ptr, data->len);
	    extension->data.ptr=&extension->buffer[pos];
	    extension->data.len=data->len;

	} else {

	    extension->data.ptr=NULL;
	    extension->data.len=0;

	}

	if (flags & SFTP_EXTENSION_FLAG_SUPPORTED) {

	    extension->flags|=SFTP_EXTENSION_FLAG_SUPPORTED;
	    extension->send_extension=_send_sftp_extension_default;

	}

	add_list_element_last(&extensions->header, &extension->list);
	extension->nr=extensions->count;
	extensions->count++;
	logoutput("add_sftp_extension: added %.*s", name->len, name->ptr);
	if (compare_ssh_string(name, 'c', SFTP_EXTENSION_NAME_STATVFS)==0) extensions->statvfs=extension;
	if (compare_ssh_string(name, 'c', SFTP_EXTENSION_NAME_FSYNC)==0) extensions->fsync=extension;

    }

    return extension;

}

static int match_extension_byname(struct list_element_s *list, void *ptr)
{
    char *name=(char *) ptr;
    struct sftp_protocolextension_s *extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));
    return compare_ssh_string(&extension->name, 'c', ptr);
}

static int compare_extension_data(struct ssh_string_s *e, struct ssh_string_s *d)
{
    int result=-1;

    if (d==NULL || d->len==0) {

	result=(e->len==0) ? 0 : -1;

    } else if (d->len==e->len) {

	result=memcmp(d->ptr, e->ptr, d->len);

    }

    return result;
}

static struct sftp_protocolextension_s *lookup_sftp_extension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data, unsigned int flags)
{
    struct list_element_s *list=NULL;
    struct sftp_protocolextension_s *extension=NULL;

    list=search_list_element_forw(&sftp->extensions.header, match_extension_byname, (void *) name);

    if (list) {

	extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	if (flags & SFTP_EXTENSION_FLAG_SUPPORTED) {

	    (* extension->event_cb)(&extension->name, &extension->data, extension->ptr, SFTP_EXTENSION_EVENT_SUPPORTED);
	    extension->send_extension=_send_sftp_extension_default;

	}

	if (compare_extension_data(&extension->data, data)==-1 && (flags & SFTP_EXTENSION_FLAG_OVERRIDE_DATA)) {
	    unsigned int len=name->len + (data) ? data->len : 0;
	    struct list_element_s *prev=get_prev_element(list);

	    if (extension->data.len != ((data) ? data->len : 0)) {

		remove_list_element(list);

		/* realloc the extension to create/remove space for the data */

		extension=realloc(extension, sizeof(struct sftp_protocolextension_s) + len);

		if (extension) {

		    if (data) {
			char *pos=&extension->buffer[name->len];

			memcpy(pos, data->ptr, data->len);
			extension->data.ptr=pos;
			extension->data.len=data->len;

		    } else {

			extension->data.ptr=NULL;
			extension->data.len=0;

		    }

		    list=&extension->list; /* address can be changed by realloc*/

		    /* put back on list after prev
			this also works when prev is empty */

		    add_list_element_after(&sftp->extensions.header, prev, list);
		    (* extension->event_cb)(&extension->name, &extension->data, extension->ptr, SFTP_EXTENSION_EVENT_DATA);

		} else {

		    (* extension->event_cb)(&extension->name, &extension->data, extension->ptr, SFTP_EXTENSION_EVENT_ERROR);
		    return NULL;

		}

	    } else if (extension->data.len>0) {

		memcpy(extension->data.ptr, data->ptr, data->len);

	    }

	}

	if (flags & SFTP_EXTENSION_FLAG_SUPPORTED) extension->flags|=SFTP_EXTENSION_FLAG_SUPPORTED;

    } else if (flags & SFTP_EXTENSION_FLAG_CREATE) {

	extension=_add_sftp_extension(sftp, name, data, flags);

    }

    return extension;
}

struct sftp_protocolextension_s *register_sftp_protocolextension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data,
				    void (* event_cb)(struct ssh_string_s *name, struct ssh_string_s *data, void *ptr, unsigned int event), void *ptr2)
{
    struct sftp_protocolextension_s *extension=lookup_sftp_extension(sftp, name, data, SFTP_EXTENSION_FLAG_CREATE);

    if (extension) {

	extension->event_cb=event_cb;
	extension->ptr=ptr2;

    }

    return extension;
}

struct sftp_protocolextension_s *add_sftp_protocolextension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data)
{
    return lookup_sftp_extension(sftp, name, data, SFTP_EXTENSION_FLAG_CREATE | SFTP_EXTENSION_FLAG_SUPPORTED | SFTP_EXTENSION_FLAG_OVERRIDE_DATA);
}

static int check_sftp_extension_mapped(struct sftp_client_s *sftp, unsigned char nr)
{
    struct list_element_s *list=NULL;
    struct sftp_protocolextension_s *extension=NULL;
    int result=-1;

    list=get_list_head(&sftp->extensions.header, 0);

    while (list) {

	extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	if (extension->mapped==nr) {

	    result=0;
	    break;

	}

	list=get_next_element(list);

    }

    return result;

}

static int map_sftp_protocolextension(struct sftp_client_s *sftp, struct sftp_protocolextension_s *mapextension, struct sftp_protocolextension_s *extension)
{
    struct sftp_request_s sftp_r;
    int result=-1;
    unsigned int error=EIO;
    unsigned int len=extension->name.len;
    unsigned char data[len + 4];
    unsigned char *pos=data;

    /* send the name of the extension to map */

    store_uint32((char *)pos, len);
    pos+=4;
    memcpy(pos, extension->name.ptr, len);
    pos+=len;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;

    sftp_r.call.extension.data=data;
    sftp_r.call.extension.size=len + 4;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    /* test the map extension by mapping itself */

    if ((* mapextension->send_extension)(sftp, mapextension, &sftp_r, &error)==0) {
	struct timespec timeout;

	get_sftp_request_timeout(sftp, &timeout);

	if (wait_sftp_response(sftp, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_EXTENDED_REPLY) {

		if (reply->response.extension.size>=4) {
		    unsigned int mapped=get_uint32((char *) reply->response.extension.buff);

		    /* only range 210-255 is allowed */

		    if (mapped>=SSH_FXP_MAPPING_MIN && mapped<=SSH_FXP_MAPPING_MAX) {
			unsigned char nr=(unsigned char) mapped;

			/* here also a check the mapped nr is not already in use */

			if (check_sftp_extension_mapped(sftp, nr)==-1) {

			    extension->mapped=nr;
			    extension->send_extension=_send_sftp_extension_custom;
			    logoutput("map_sftp_protocolextension: %.*s mapped to nr %i", extension->name.len, extension->name.ptr, mapped);
			    result=0;

			} else {

			    logoutput("map_sftp_protocolextension: nr %i already in use", mapped);

			}

		    } else {

			logoutput("map_sftp_protocolextension: received illegal nr %i", mapped);
			error=EPROTO;

		    }

		} else {

		    logoutput("map_sftp_protocolextension: response size too small: %i", reply->response.extension.size);
		    error=EPROTO;

		}

	    } else if (reply->type==SSH_FXP_STATUS) {

		logoutput("map_sftp_protocolextension: error response : %i (%s)", reply->response.status.linux_error, strerror(reply->response.status.linux_error));
		error=reply->response.status.linux_error;

	    }

	}

    }

    return result;
}

void complete_sftp_protocolextensions(struct sftp_client_s *sftp, char *mapname)
{
    struct list_element_s *list=NULL;

    if (sftp->extensions.mapextension==NULL) {
	char *mapname=NULL;
	struct ctx_option_s option;

	memset(&option, 0, sizeof(struct ctx_option_s));

	if ((* sftp->context.signal_sftp2ctx)(sftp, "option:sftp.mapextension.name", &option)>=0) {

	    if (option.type==_CTX_OPTION_TYPE_PCHAR) mapname=(char *) option.value.name;

	}

	if (mapname==NULL) mapname=NAME_MAPEXTENSION_DEFAULT;

	/* is mapping supported by server ? 
	    are there well known mapping extensions?
	    which mappings to try */
	list=search_list_element_forw(&sftp->extensions.header, match_extension_byname, (void *) mapname);

	if (list) {
	    struct sftp_protocolextension_s *extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	    if (map_sftp_protocolextension(sftp, extension, extension)==0) {

		logoutput("complete_sftp_protocolextension: mapping success");
		sftp->extensions.mapextension=extension;

	    } else {

		logoutput("complete_sftp_protocolextension: mapping failed");

	    }

	}

    }

    if (sftp->extensions.mapextension) {

	/* map all other extensions */

	list=get_list_head(&sftp->extensions.header, 0);

	while (list) {

	    struct sftp_protocolextension_s *extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	    if (extension != sftp->extensions.mapextension) {

		if (map_sftp_protocolextension(sftp, sftp->extensions.mapextension, extension)==0) {

		    logoutput("complete_sftp_protocolextension: mapping of %.*s success to %i", extension->name.len, extension->name.ptr, extension->mapped);
		    (* extension->event_cb)(&extension->name, &extension->data, extension->ptr, SFTP_EXTENSION_EVENT_MAPPED);

		} else {

		    logoutput("complete_sftp_protocolextension: mapping of %.*s failed", extension->name.len, extension->name.ptr);

		}

	    }

	    list=get_next_element(list);

	}

    }

    if (sftp->extensions.statvfs==NULL) {
	struct list_element_s *list=NULL;

	list=search_list_element_forw(&sftp->extensions.header, match_extension_byname, (void *) SFTP_EXTENSION_NAME_STATVFS);

	if (list) {
	    struct sftp_protocolextension_s *extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	    sftp->extensions.statvfs=extension;

	}

    }

    if (sftp->extensions.fsync==NULL) {
	struct list_element_s *list=NULL;

	list=search_list_element_forw(&sftp->extensions.header, match_extension_byname, (void *) SFTP_EXTENSION_NAME_FSYNC);

	if (list) {
	    struct sftp_protocolextension_s *extension=(struct sftp_protocolextension_s *)(((char *) list) - offsetof(struct sftp_protocolextension_s, list));

	    sftp->extensions.fsync=extension;

	}

    }

}


static void fs_sftp_extension_event_cb(struct ssh_string_s *name, struct ssh_string_s *data, void *ptr, unsigned int event)
{
    switch (event) {

    case SFTP_EXTENSION_EVENT_SUPPORTED:

	logoutput("fs_sftp_extension_event_cb: %.*s supported by server", name->len, name->ptr);

    case SFTP_EXTENSION_EVENT_DATA:

	logoutput("fs_sftp_extension_event_cb: data received for %.*s", name->len, name->ptr);

    case SFTP_EXTENSION_EVENT_MAPPED:

	logoutput("fs_sftp_extension_event_cb: mapping %.*s", name->len, name->ptr);

    case SFTP_EXTENSION_EVENT_ERROR:

	break;

    }

}

/*
    register some fs extensions are supported and if so, try to map these */

void init_fs_sftp_extensions(struct sftp_client_s *sftp)
{
    struct ssh_string_s name=SSH_STRING_INIT;

    /* register the statvfs extension */

    name.len=strlen(SFTP_EXTENSION_NAME_STATVFS);
    name.ptr=SFTP_EXTENSION_NAME_STATVFS;
    register_sftp_protocolextension(sftp, &name, NULL, fs_sftp_extension_event_cb, NULL);
    clear_ssh_string(&name);

    /* register the fsync extension */

    name.len=strlen(SFTP_EXTENSION_NAME_FSYNC);
    name.ptr=SFTP_EXTENSION_NAME_FSYNC;
    register_sftp_protocolextension(sftp, &name, NULL, fs_sftp_extension_event_cb, NULL);
    clear_ssh_string(&name);

    /* more ? like */

    /*
    - posix-rename@openssh.com
    - fstatvfs@openssh.com (not required by fuse)
    - hardlink@openssh.com
    - backup related extensions
    - opendir@sftp.bononline.nl
    */
}

int send_sftp_fsync(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_protocolextension_s *extension=sftp->extensions.fsync;
    unsigned char *handle=sftp_r->call.fsync.handle;
    unsigned int len=sftp_r->call.fsync.len;
    unsigned char data[len + 4];
    unsigned char *pos=data;

    /* transform into extension format */

    store_uint32((char *)pos, len);
    pos+=4;
    memcpy(pos, handle, len);
    pos+=len;

    sftp_r->call.extension.data=data;
    sftp_r->call.extension.size=len + 4;

    return (* extension->send_extension)(sftp, extension, sftp_r, error);
}

int send_sftp_statvfs(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_protocolextension_s *extension=sftp->extensions.statvfs;
    unsigned char *path=sftp_r->call.statvfs.path;
    unsigned int len=sftp_r->call.statvfs.len;
    unsigned char data[len + 4];
    unsigned char *pos=data;

    /* transform into extension format */

    store_uint32((char *)pos, len);
    pos+=4;
    memcpy(pos, path, len);
    pos+=len;

    sftp_r->call.extension.data=data;
    sftp_r->call.extension.size=len + 4;

    logoutput_debug("send_sftp_statvfs: ext def %s", (extension) ? "def" : "undef");

    return (* extension->send_extension)(sftp, extension, sftp_r, error);
}

void set_sftp_statvfs_unsupp(struct sftp_client_s *sftp)
{
    struct sftp_protocolextension_s *extension=sftp->extensions.statvfs;
    extension->send_extension=_send_sftp_extension_notsupp;
}

void set_sftp_fsync_unsupp(struct sftp_client_s *sftp)
{
    struct sftp_protocolextension_s *extension=sftp->extensions.fsync;
    extension->send_extension=_send_sftp_extension_notsupp;
}


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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "sftp/common-protocol.h"
#include "common.h"
#include "request-hash.h"

#define NAME_MAPEXTENSION_DEFAULT			"mapextension@bononline.nl"
#define SFTP_EXTENSION_NAME_STATVFS			"statvfs@openssh.com"
#define SFTP_EXTENSION_NAME_FSYNC			"fsync@openssh.com"

#include "extensions/fsync.h"
#include "extensions/statvfs.h"

static int _send_sftp_extension_default(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *extension, struct sftp_request_s *sftp_r)
{
    struct sftp_protocol_extension_server_s *esbs=extension->esbs;

    memset(&sftp_r->call.extension.id, 0, sizeof(sftp_r->call.extension.id));
    sftp_r->call.extension.type=SFTP_EXTENSION_TYPE_DEFAULT;
    sftp_r->call.extension.id.name.len=esbs->name.len;
    sftp_r->call.extension.id.name.name=(unsigned char *) esbs->name.ptr;
    return (* sftp->send_ops->extension)(sftp, sftp_r);
}

static int _send_sftp_extension_custom(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *extension, struct sftp_request_s *sftp_r)
{
    memset(&sftp_r->call.extension.id, 0, sizeof(sftp_r->call.extension.id));
    sftp_r->call.extension.type=SFTP_EXTENSION_TYPE_CUSTOM;
    sftp_r->call.extension.id.nr=extension->code;
    return (* sftp->send_ops->custom)(sftp, sftp_r);
}

static int _send_sftp_extension_notsupp(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *extension, struct sftp_request_s *sftp_r)
{
    return -EOPNOTSUPP;
}

void init_sftp_extensions(struct sftp_client_s *sftp)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;

    extensions->aextensions=NULL;
    extensions->size=0;

    init_list_header(&extensions->supported, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&extensions->supported_server, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&extensions->supported_client, SIMPLE_LIST_TYPE_EMPTY, NULL);

    register_client_sftp_extension_fsync(extensions);
    register_client_sftp_extension_statvfs(extensions);

}

void clear_sftp_extensions(struct sftp_client_s *sftp)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct list_element_s *list=NULL;

    /* free the list with extensions received from server */

    list=remove_list_head(&extensions->supported_server);

    while (list) {
	struct sftp_protocol_extension_server_s *esbs=(struct sftp_protocol_extension_server_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_server_s, list));

	clear_ssh_string(&esbs->name);
	clear_ssh_string(&esbs->data);
	free(esbs);

	list=remove_list_head(&extensions->supported_server);

    }

    /* free the array with extensions used (supported by server and client) */

    if (extensions->aextensions) {

	free(extensions->aextensions);
	extensions->aextensions=NULL;

    }

}

struct sftp_protocol_extension_server_s *find_protocol_extension_server(struct sftp_client_s *sftp, struct ssh_string_s *name)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct sftp_protocol_extension_server_s *esbs=NULL;
    struct list_element_s *list=NULL;

    list=get_list_head(&extensions->supported_server);

    while (list) {

	esbs=(struct sftp_protocol_extension_server_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_server_s, list));
	if (compare_ssh_string(&esbs->name, 's', (void *) name)==0) break;
	list=get_next_element(list);
	esbs=NULL;

    }

    return esbs;

}

struct sftp_protocol_extension_client_s *find_protocol_extension_client(struct sftp_client_s *sftp, struct ssh_string_s *name)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct sftp_protocol_extension_client_s *esbc=NULL;
    struct list_element_s *list=NULL;

    list=get_list_head(&extensions->supported_client);

    while (list) {

	esbc=(struct sftp_protocol_extension_client_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_client_s, list));
	if (compare_ssh_string(&esbc->name, 's', (void *) name)==0) break;
	list=get_next_element(list);
	esbc=NULL;

    }

    return esbc;

}

/* create an extension on server list
    typically used */

int add_protocol_extension_server(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data)
{
    struct sftp_protocol_extension_server_s *esbs=NULL;

    if (name) {

	logoutput("add_protocol_extension_server: add %.*s ", name->len, name->ptr);

    } else {

	logoutput("add_protocol_extension_server: invalid paramers ... name not defined");
	return SFTP_EXTENSION_ADD_RESULT_ERROR;

    }

    esbs=find_protocol_extension_server(sftp, name);

    if (esbs) {

	logoutput("add_protocol_extension_server: %.*s does exist already", name->len, name->ptr);
	return SFTP_EXTENSION_ADD_RESULT_EXIST;

    }

    esbs=malloc(sizeof(struct sftp_protocol_extension_server_s));

    if (esbs) {
	struct sftp_extensions_s *extensions=&sftp->extensions;
	struct ssh_string_s *to=NULL;

	memset(esbs, 0, sizeof(struct sftp_protocol_extension_server_s));
	init_list_element(&esbs->list, NULL);
	init_ssh_string(&esbs->name);
	init_ssh_string(&esbs->data);

	to=&esbs->name;
	create_copy_ssh_string(&to, name);

	if (data && data->len>0) {

	    to=&esbs->data;
	    create_copy_ssh_string(&to, data);

	}

	add_list_element_last(&extensions->supported_server, &esbs->list);
	logoutput("add_protocol_extension_server: %.*s added", name->len, name->ptr);
	return SFTP_EXTENSION_ADD_RESULT_CREATED;

    }

    return SFTP_EXTENSION_ADD_RESULT_ERROR;

}

static void init_sftp_protocol_extension(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *ext, unsigned int size, struct sftp_protocol_extension_client_s *esbc)
{

    memset(ext, 0, sizeof(struct sftp_protocol_extension_s) + size);

    ext->esbc=esbc;
    ext->esbs=NULL;
    ext->code=0;
    ext->size=size;
    init_list_element(&ext->list, NULL);

    if (esbc) {

	(* esbc->init)(sftp, ext);
	ext->send=_send_sftp_extension_default;

    } else {

	ext->send=_send_sftp_extension_notsupp;

    }

}

struct sftp_protocol_extension_s *create_sftp_protocol_extension(struct sftp_client_s *sftp, struct ssh_string_s *name, int *result)
{
    struct sftp_protocol_extension_s *ext=NULL;
    struct sftp_protocol_extension_server_s *esbs=NULL;
    struct sftp_protocol_extension_client_s *esbc=NULL;
    unsigned int size=0;

    /* check it's supported by server */

    esbs=find_protocol_extension_server(sftp, name);

    if (esbs==NULL) {

	logoutput_warning("create_sftp_protocol_extension: %.*s not supported by server.", name->len, name->ptr);
	if (result) *result=SFTP_EXTENSION_ADD_RESULT_NO_SUPP_SERVER;
	return NULL;

    }

    esbc=find_protocol_extension_client(sftp, name);

    if (esbc==NULL) {

	logoutput_warning("create_sftp_protocol_extension: %.*s not supported by client.", name->len, name->ptr);
	if (result) *result=SFTP_EXTENSION_ADD_RESULT_NO_SUPP_CLIENT;
	return NULL;

    }

    size=(* esbc->get_size_buffer)(sftp, &esbs->name, &esbs->data);
    ext=malloc(sizeof(struct sftp_protocol_extension_s) + size);

    if (ext) {

	init_sftp_protocol_extension(sftp, ext, size, esbc);
	ext->esbs=esbs;

    } else {

	if (result) *result=SFTP_EXTENSION_ADD_RESULT_ERROR;

    }

    return ext;

}

static int send_sftp_extension_generic(struct sftp_client_s *sftp, struct sftp_protocol_extension_s *ext, struct sftp_request_s *sftp_r)
{
    struct sftp_protocol_extension_client_s *esbc=ext->esbc;
    unsigned int len=(* esbc->get_data_len)(sftp, ext, sftp_r);
    char data[len];

    len=(* esbc->fill_data)(sftp, ext, sftp_r, data, len);

    /* use the extension of the call union to send */

    sftp_r->call.extension.data=(unsigned char *)data;
    sftp_r->call.extension.size=len;

    return (* ext->send)(sftp, ext, sftp_r);
}

int send_sftp_extension_index(struct sftp_client_s *sftp, unsigned int index, struct sftp_request_s *sftp_r)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;

    if (index < extensions->size) {
	struct sftp_protocol_extension_s *ext=extensions->aextensions[index];

	return send_sftp_extension_generic(sftp, ext, sftp_r);

    }

    return -1;

}

unsigned int get_sftp_protocol_extension_index(struct sftp_client_s *sftp, struct ssh_string_s *name)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct sftp_protocol_extension_s *ext=NULL;
    unsigned int result=0;
    unsigned int index=0;

    while (index < extensions->size) {

	ext=extensions->aextensions[index];

	if ((ext->esbs) && (compare_ssh_string(&ext->esbs->name, 's', (void *) name)==0)) {

	    result=index;
	    break;

	}

	index++;
	ext=NULL;

    }

    logoutput("get_sftp_protocol_extension_index: extension %.*s index %u", name->len, name->ptr, result);
    return result;
}

struct ssh_string_s *get_sftp_protocol_extension_name(struct sftp_client_s *sftp, unsigned int index)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct sftp_protocol_extension_s *ext=NULL;
    struct ssh_string_s *name=NULL;

    if (index < extensions->size) {

	ext=extensions->aextensions[index];
	if (ext->esbs) name=&ext->esbs->name;

    }

    return name;

}

unsigned int get_sftp_protocol_extension_count(struct sftp_client_s *sftp)
{
    return sftp->extensions.size;
}

struct ssh_string_s *get_supported_next_extension_name(struct sftp_client_s *sftp, struct list_element_s **p_list, unsigned char who)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct list_element_s *list=*p_list;
    struct ssh_string_s *name=NULL;

    if (list) {

	list=get_next_element(list);

    } else {
	struct list_header_s *header=((who>0) ? &extensions->supported_server : &extensions->supported_client);

	list=get_list_head(header);

    }

    *p_list=list;
    if (list) {

	if (who>0) {
	    struct sftp_protocol_extension_server_s *esbs=(struct sftp_protocol_extension_server_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_server_s, list));

	    name=&esbs->name;

	} else {
	    struct sftp_protocol_extension_client_s *esbc=(struct sftp_protocol_extension_client_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_client_s, list));

	    name=&esbc->name;

	}

    }

    return name;
}

static unsigned char _cb_interrupted_dummy(void *ptr)
{
    return 0;
}

void map_sftp_extension(struct sftp_client_s *sftp, unsigned int index)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;

    if (index < extensions->size) {
	struct sftp_request_s sftp_r;
	struct sftp_protocol_extension_s *ext=extensions->aextensions[index];
	struct ssh_string_s *name=&ext->esbc->name;
	unsigned int size=4 + name->len + 1; /* space to write name of extension to map plus 1 byte */
	char buffer[size];
	unsigned int pos=0;

	pos += write_ssh_string(buffer, size, 's', (void *) name); /* the name of extension to map */
	buffer[pos]=1; /* set last byte to 1 to map */

	memset(&sftp_r, 0, sizeof(struct sftp_request_s));

	sftp_r.status = SFTP_REQUEST_STATUS_WAITING;
	sftp_r.interface=NULL;
	sftp_r.unique=0;
	sftp_r.send=send_sftp_request_data_default;
	sftp_r.ptr=NULL;
	sftp_r.call.extension.size=size;
	sftp_r.call.extension.data=buffer;

	if (send_sftp_extension_generic(sftp, ext, &sftp_r)>0) {
	    struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	    get_sftp_request_timeout(sftp, &timeout);

	    if (wait_sftp_response(sftp, &sftp_r, &timeout, _cb_interrupted_dummy, NULL)==1) {
		struct sftp_reply_s *reply=&sftp_r.reply;

		if (reply->type==SSH_FXP_EXTENDED_REPLY) {

		    if (reply->size>=4) {

			uint32_t code2use=get_uint32(reply->data);

			logoutput_debug("map_sftp_extension: received code %u", code2use);

			/* here some check ?? */

			if (code2use<=255) {

			    ext->flags |= SFTP_EXTENSION_FLAG_MAPPED;
			    ext->code = (unsigned char) code2use;
			    ext->send = _send_sftp_extension_custom;

			}

		    } else {

			logoutput_debug("map_sftp_extension: received buffer too small (size=%u)", reply->size);

		    }

		} else if (reply->type==SSH_FXP_STATUS) {

		    unsigned int errcode=reply->response.status.linux_error;
		    logoutput_debug("map_sftp_extension: received error %u:%s", errcode, strerror(errcode));

		}

	    }

	}

    }

}

int complete_sftp_extensions(struct sftp_client_s *sftp)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    unsigned int count=(unsigned int) extensions->supported_client.count; /* an indication of the amount of extensions to be used */
    struct sftp_protocol_extension_s *ext=NULL;
    struct sftp_protocol_extension_s *mapext=NULL;
    struct list_element_s *list=NULL;
    int result=0;

    /* add the "unsupported" extension */

    ext=malloc(sizeof(struct sftp_protocol_extension_s));

    if (ext) {

	init_sftp_protocol_extension(sftp, ext, 0, NULL);
	add_list_element_last(&extensions->supported, &ext->list);

    } else {

	logoutput_debug("complete_sftp_extensions: unable to allocate memory for protocol extension ... cannot continue");
	return -1;

    }

    /* create a list of extensions supported by client and server */

    list=get_list_head(&extensions->supported_client);

    while (list) {
	struct sftp_protocol_extension_client_s *esbc=(struct sftp_protocol_extension_client_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_client_s, list));
	struct sftp_protocol_extension_server_s *esbs=NULL;
	struct ssh_string_s *name=&esbc->name;

	/* look for extension supported by server with the same name
	    TODO: look for extensions with the same function ... */

	esbs=find_protocol_extension_server(sftp, name);

	if (esbs) {
	    unsigned int size=(* esbc->get_size_buffer)(sftp, name, &esbs->data); /* size for additional data required by extension ... */

	    ext=malloc(sizeof(struct sftp_protocol_extension_s) + size);

	    if (ext) {

		logoutput_debug("complete_sftp_extensions: created extension %.*s", name->len, name->ptr);
		init_sftp_protocol_extension(sftp, ext, size, esbc);
		add_list_element_last(&extensions->supported, &ext->list);
		ext->esbs=esbs;

		if ((sftp->flags & SFTP_CLIENT_FLAG_MAPEXTENSIONS) && (compare_ssh_string(name, 'c', NAME_MAPEXTENSION_DEFAULT)==0)) mapext=ext;

	    } else {

		logoutput_debug("complete_sftp_extensions: unable to allocate memory for protocol extension ... cannot continue");
		return -1;

	    }

	}

	list=get_next_element(list);

    }

    if (extensions->supported.count>1) {
	unsigned int len=sizeof(struct sftp_protocol_extension_s *);
	struct sftp_protocol_extension_s **aext=malloc(extensions->supported.count * len);

	/* create a fast lookup array: the index is an entry to access the extensions */

	if (aext) {

	    list=get_list_head(&extensions->supported);

	    for (unsigned int i=0; i<extensions->supported.count; i++) {

		aext[i]=(struct sftp_protocol_extension_s *)(((char *)list) - offsetof(struct sftp_protocol_extension_s, list));
		list=get_next_element(list);

	    }

	    extensions->aextensions=aext;
	    extensions->size=extensions->supported.count;

	} else {

	    logoutput_warning("complete_sftp_extensions: unable to allocate memory (%u x %u bytes)", extensions->supported.count, len);
	    result=-1;

	}

    } else {

	logoutput_warning("complete_sftp_extensions: no extensions found shared by server and client");

    }

    /* here: free the list with extensions supported by server ??
	only when client will not register any more extensions and all well known and useable extensions are initialized */

    /* do remmping of the registered sftp protocol extensions? */

    if (mapext && extensions->aextensions) {

	for (unsigned int i=1; i<extensions->supported.count; i++) {

	    ext=extensions->aextensions[i];

	    /* do not map ourselves */

	    if (ext != mapext) map_sftp_extension(sftp, i);

	}

    }

    return result;

}


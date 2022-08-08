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
#include "libosns-resources.h"

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
    sftp_r->status=SFTP_REQUEST_STATUS_WAITING;
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

    list=get_list_head(&extensions->supported_server, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct sftp_protocol_extension_server_s *esbs=(struct sftp_protocol_extension_server_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_server_s, list));

	clear_ssh_string(&esbs->name);
	clear_ssh_string(&esbs->data);
	free(esbs);

	list=get_list_head(&extensions->supported_server, SIMPLE_LIST_FLAG_REMOVE);

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

    logoutput_debug("find_protocol_extension_server: look for %.*s", name->len, name->ptr);

    list=get_list_head(&extensions->supported_server, 0);

    while (list) {

	esbs=(struct sftp_protocol_extension_server_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_server_s, list));
	logoutput_debug("find_protocol_extension_server: found %.*s", esbs->name.len, esbs->name.ptr);
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

    list=get_list_head(&extensions->supported_client, 0);

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

    ext->flags=0;
    ext->esbc=esbc;
    ext->esbs=NULL;
    ext->code=0;
    ext->size=size;

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
	struct sftp_protocol_extension_s *ext=&extensions->aextensions[index];

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

	ext=&extensions->aextensions[index];

	if ((ext->esbs) && (compare_ssh_string(&ext->esbs->name, 's', (void *) name)==0)) {

	    result=index;
	    break;

	}

	index++;
	ext=NULL;

    }

    logoutput("get_sftp_protocol_extension_index: extension %.*s index %u", name->len, name->ptr, index);

    return index;
}

struct ssh_string_s *get_sftp_protocol_extension_name(struct sftp_client_s *sftp, unsigned int index)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    struct sftp_protocol_extension_s *ext=NULL;
    struct ssh_string_s *name=NULL;

    if (index < extensions->size) {

	ext=&extensions->aextensions[index];
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

	list=get_list_head(header, 0);

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

int complete_sftp_extensions(struct sftp_client_s *sftp)
{
    struct sftp_extensions_s *extensions=&sftp->extensions;
    unsigned int count=(unsigned int) extensions->supported_client.count;
    struct sftp_protocol_extension_s tmp[count + 1];
    struct list_element_s *list=NULL;
    unsigned int index=0;
    int result=0;

    /* add the "unsupported" extension */

    init_sftp_protocol_extension(sftp, &tmp[0], 0, NULL);
    index++;

    /* create a list of extensions supported by client and server */

    list=get_list_head(&extensions->supported_client, 0);

    while (list) {
	struct sftp_protocol_extension_client_s *esbc=(struct sftp_protocol_extension_client_s *)(((char *) list) - offsetof(struct sftp_protocol_extension_client_s, list));

	/* look for extension supported by server with the same name */

	struct sftp_protocol_extension_server_s *esbs=find_protocol_extension_server(sftp, &esbc->name);

	if (esbs) {
	    unsigned int size=(* esbc->get_size_buffer)(sftp, &esbs->name, &esbs->data);

	    init_sftp_protocol_extension(sftp, &tmp[index], size, esbc);
	    tmp[index].esbs=esbs;
	    index++;

	}

	list=get_next_element(list);

    }

    if (index>0) {
	unsigned int len=sizeof(struct sftp_protocol_extension_s);
	struct sftp_protocol_extension_s *aext=malloc(index * len);

	if (aext) {

	    memcpy(aext, tmp, index * len);
	    extensions->aextensions=aext;
	    extensions->size=index;
	    result=(int) index;

	} else {

	    logoutput_warning("complete_sftp_extensions: unable to allocate memory (%u x %u bytes)", index, len);
	    result=-1;

	}

    } else {

	logoutput_warning("complete_sftp_extensions: no extensions found shared by server and client");

    }

    return result;

}

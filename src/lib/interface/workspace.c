/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "log.h"
#include "workspace.h"

extern struct context_interface_s *get_next_context_interface(struct context_interface_s *reference, struct context_interface_s *interface);
static struct list_header_s interface_ops_list=INIT_LIST_HEADER;
static unsigned int unique_ctr=1;
static pthread_mutex_t mutex_unique=PTHREAD_MUTEX_INITIALIZER;

static void _free_option_alloc(struct ctx_option_s *option)
{
    if (option->type==_CTX_OPTION_TYPE_BUFFER && (option->flags & _CTX_OPTION_FLAG_ALLOC)) {

	if (option->value.buffer.ptr) {

	    free(option->value.buffer.ptr);
	    option->value.buffer.ptr=NULL;

	}

    }

}

void init_ctx_option(struct ctx_option_s *option, unsigned char type)
{
    memset(option, 0, sizeof(struct ctx_option_s));
    option->free=_free_option_alloc;
    option->type=type;
}

unsigned char ctx_option_error(struct ctx_option_s *option)
{
    return (option->flags & _CTX_OPTION_FLAG_ERROR) ? 1 : 0;
}

unsigned char ctx_option_buffer(struct ctx_option_s *option)
{
    return (option->type==_CTX_OPTION_TYPE_BUFFER && option->value.buffer.len>0) ? 1 : 0;
}

unsigned char ctx_option_uint(struct ctx_option_s *option)
{
    return (option->type==_CTX_OPTION_TYPE_INT) ? 1 : 0;
}

unsigned char ctx_option_valid(struct ctx_option_s *option)
{
    return (option->flags & _CTX_OPTION_FLAG_VALID) ? 1 : 0;
}

char *ctx_option_get_buffer(struct ctx_option_s *option, unsigned int *len)
{
    if (len) *len=option->value.buffer.len;
    return option->value.buffer.ptr;
}

unsigned int ctx_option_get_uint(struct ctx_option_s *option)
{
    return option->value.integer;
}

void ctx_option_free(struct ctx_option_s *option)
{
    (* option->free)(option);
}

static int _connect_interface(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    return -1;
}

static void _free_interface(struct context_interface_s *interface)
{
    /* for default there is nothing extra to be freed and the interface is never occupied/used
        so it's safe to free */

}

static int _start_interface(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    return -1;
}

static int _signal_nothing(struct context_interface_s *interface, const char *what, struct ctx_option_s *o)
{
    return -1;
}

static char *_get_interface_buffer(struct context_interface_s *interface)
{
    return NULL;
}

int init_context_interface(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{

    memset(interface, 0, sizeof(struct context_interface_s));
    interface->type=0;
    interface->flags=0;

    pthread_mutex_lock(&mutex_unique);
    interface->unique=unique_ctr;
    unique_ctr++;
    pthread_mutex_unlock(&mutex_unique);

    interface->connect=_connect_interface;
    interface->start=_start_interface;
    interface->free=_free_interface;
    interface->signal_context=_signal_nothing;
    interface->signal_interface=_signal_nothing;
    interface->get_interface_buffer=_get_interface_buffer;

    if (ilist) {
	struct interface_ops_s *ops=ilist->ops;

	if ((*ops->init_buffer)(interface, ilist, primary)==-1) {

	    logoutput("set_context_interface: initializing buffer failed for type %s", ops->name);
	    return -1;

	}

    }

    return 0;
}

void reset_context_interface(struct context_interface_s *interface)
{
    // (* interface->free_backend_data)(interface);
    init_context_interface(interface, NULL, NULL);
}

void add_interface_ops(struct interface_ops_s *ops)
{
    add_list_element_last(&interface_ops_list, &ops->list);
}

static struct interface_ops_s *get_interface_ops_container(struct list_element_s *list)
{
    return (struct interface_ops_s *)((char *) list - offsetof(struct interface_ops_s, list));
}

struct interface_ops_s *get_next_interface_ops(struct interface_ops_s *ops)
{
    struct list_element_s *next=((ops) ? get_next_element(&ops->list) : get_list_head(&interface_ops_list, 0));
    return ((next) ? get_interface_ops_container(next) : NULL);
}

unsigned int build_interface_ops_list(struct context_interface_s *interface, struct interface_list_s *ilist, unsigned int start)
{
    struct interface_ops_s *ops=get_next_interface_ops(NULL);

    while (ops) {

	start=(* ops->populate)(interface, ops, ilist, start);
	ops=get_next_interface_ops(ops);

    }

    return start;
}

struct interface_list_s *get_interface_list(struct interface_list_s *ailist, unsigned int count, int type)
{

    for (unsigned int i=0; i<count; i++) {

	if (ailist[i].type==type) return &ailist[i];

    }

    return NULL;
}

void init_interfaces()
{
    init_list_header(&interface_ops_list, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

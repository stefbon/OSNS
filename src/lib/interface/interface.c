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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"

#include "iocmd.h"
#include "interface.h"
#include "list.h"

static int _connect_interface(struct context_interface_s *i, union interface_target_u *t, union interface_parameters_u *p)
{
    return -1;
}

static int _start_interface(struct context_interface_s *i)
{
    return -1;
}

static int _signal_nothing(struct context_interface_s *i, const char *what, struct io_option_s *o, struct context_interface_s *s, unsigned int type)
{
    return 0;
}

static char *get_interface_buffer_default(struct context_interface_s *i)
{
    return i->buffer;
}

static struct list_header_s *get_header_connections_default(struct context_interface_s *i)
{
    return NULL;
}

static unsigned int get_interface_status_default(struct context_interface_s *i, struct interface_status_s *status)
{
    return 0;
}

static void set_primary_default(struct context_interface_s *i, struct context_interface_s *p)
{
    i->link.primary=p;
    i->flags |= _INTERFACE_FLAG_SECONDARY_1TON;
}

static void set_secondary_default(struct context_interface_s *i, struct context_interface_s *s)
{
    i->link.secondary.refcount++;
    i->flags |= _INTERFACE_FLAG_PRIMARY_1TON;
}

static struct context_interface_s *get_primary_default(struct context_interface_s *i)
{
    return ((i->flags & _INTERFACE_FLAG_SECONDARY) ? i->link.primary : NULL);
}

void init_context_interface(struct context_interface_s *i, unsigned int type, unsigned int size)
{

    i->type=type;
    i->flags=0;
    i->unique=0;
    i->ptr=NULL;

    i->connect=_connect_interface;
    i->start=_start_interface;
    i->get_interface_buffer=get_interface_buffer_default;
    i->get_header_connections=get_header_connections_default;
    i->get_interface_status=get_interface_status_default;
    i->set_primary=set_primary_default;
    i->set_secondary=set_secondary_default;
    i->get_primary=get_primary_default;

    i->iocmd.in=_signal_nothing;
    i->iocmd.out=_signal_nothing;

    i->size=size;
}

void reset_context_interface(struct context_interface_s *i)
{
    init_context_interface(i, 0, 0);
}

struct context_interface_s *get_primary_context_interface(struct context_interface_s *i)
{
    return ((i->flags & (_INTERFACE_FLAG_SECONDARY_1TO1 | _INTERFACE_FLAG_SECONDARY_1TON)) ? i->link.primary : NULL);
}

void init_context_interfaces()
{
    init_header_interfaces();
}

void init_interface_status (struct interface_status_s *istatus)
{
    istatus->flags=0;
    istatus->errcode=0;
    set_system_time(&istatus->time, 0, 0);
}

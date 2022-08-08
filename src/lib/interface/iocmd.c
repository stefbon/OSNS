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

const char *get_name_interface_signal_sender(unsigned int type)
{
    const char *sender="--unknown--";

    if (type==INTERFACE_CTX_SIGNAL_TYPE_APPLICATION) {

	sender="application";

    } else if (type==INTERFACE_CTX_SIGNAL_TYPE_SSH_SESSION) {

	sender="ssh-session";

    } else if (type==INTERFACE_CTX_SIGNAL_TYPE_SSH_CHANNEL) {

	sender="ssh-channel";

    } else if (type==INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE) {

	sender="workspace";

    } else if (type==INTERFACE_CTX_SIGNAL_TYPE_REMOTE) {

	sender="remote server";

    }

    return sender;

}

static void _free_option_alloc(struct io_option_s *option)
{
    if (option->type==_IO_OPTION_TYPE_BUFFER && (option->flags & _IO_OPTION_FLAG_ALLOC)) {

	if (option->value.buffer.ptr) {

	    free(option->value.buffer.ptr);
	    option->value.buffer.ptr=NULL;

	}

    }

}

void init_io_option(struct io_option_s *option, unsigned char type)
{
    memset(option, 0, sizeof(struct io_option_s));
    option->free=_free_option_alloc;
    option->type=type;
}

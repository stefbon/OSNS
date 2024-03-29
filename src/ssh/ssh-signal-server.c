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
#include "libosns-main.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh-receive.h"
#include "ssh-connections.h"
#include "ssh-extensions.h"

/* what to do here?
    signal cb's from ssh session to main server like?*/

void init_ssh_session_signals_server(struct ssh_session_ctx_s *context)
{
}

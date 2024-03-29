/*
  2017 Stef Bon <stefbon@gmail.com>

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
#include "utils.h"

#ifdef HAVE_GIO2

#include <gio/gio.h>

char *lookupname_dns(char *ip)
{
    GInetAddress *a=g_inet_address_new_from_string((const gchar *) ip);
    GResolver *resolver=g_resolver_get_default();
    gchar *name=g_resolver_lookup_by_address(resolver, a, NULL, NULL);

    return (char *) name;
}

#else

char *lookupname_dns(char *ip)
{
    logoutput_debug("lookupname_dns: not supported");
    return NULL;
}

#endif

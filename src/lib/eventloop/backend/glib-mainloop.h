/*
  2010, 2011, 2012, 2013 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_EVENTLOOP_BACKEND_GLIB_MAINLOOP_H
#define _LIB_EVENTLOOP_BACKEND_GLIB_MAINLOOP_H

#ifdef HAVE_GLIB2
#include <glib.h>
#endif

struct _beventloop_glib_s {
#ifdef HAVE_GLIB2
    GMainLoop				*loop;
    GSourceFuncs			funcs;
#endif
};

/* Prototypes */

void set_beventloop_glib(struct beventloop_s *eloop);
int init_beventloop_glib(struct beventloop_s *loop);

#endif

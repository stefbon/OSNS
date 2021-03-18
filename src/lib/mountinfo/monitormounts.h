/*
  2010, 2011, 2012 Stef Bon <stefbon@gmail.com>

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

#ifndef ONSN_LIB_MOUNTINFO_MONITORMOUNTS_H
#define OSNS_LIB_MOUNTINFO_MONITORMOUNTS_H

int add_mountinfo_watch(struct beventloop_s *loop, unsigned int *error);
void remove_mountinfo_watch();

void add_mountinfo_fs(char *fs);
void add_mountinfo_source(char *source);

#endif

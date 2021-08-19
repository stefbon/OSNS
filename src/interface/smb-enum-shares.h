/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef INTERFACE_SMB_ENUM_SHARES_H
#define INTERFACE_SMB_ENUM_SHARES_H

/* prototypes */

#define SMB_SHARE_TYPE_DISKTREE						1
#define SMB_SHARE_TYPE_PRINTQ						2
#define SMB_SHARE_TYPE_DEVICE						3
#define SMB_SHARE_TYPE_IPC						4

#define SMB_SHARE_FLAG_TEMPORARY					1
#define SMB_SHARE_FLAG_HIDDEN						2
#define SMB_SHARE_FLAG_ERROR						4

int smb_share_enum_async_ctx(struct context_interface_s *interface, void (* cb)(struct context_interface_s *interface, char *name, unsigned int type, unsigned int flags, void *ptr), void *ptr);

#endif

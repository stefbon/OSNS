/*
  2021 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_SFTP_ACL_ACE4_H
#define LIB_SFTP_ACL_ACE4_H

/* ace type */

#define ACE4_ACCESS_ALLOWED_ACE_TYPE 		0x00000000
#define ACE4_ACCESS_DENIED_ACE_TYPE  		0x00000001
#define ACE4_SYSTEM_AUDIT_ACE_TYPE   		0x00000002
#define ACE4_SYSTEM_ALARM_ACE_TYPE   		0x00000003

/* ace flag */

#define ACE4_FILE_INHERIT_ACE           	0x00000001
#define ACE4_DIRECTORY_INHERIT_ACE      	0x00000002
#define ACE4_NO_PROPAGATE_INHERIT_ACE   	0x00000004
#define ACE4_INHERIT_ONLY_ACE           	0x00000008
#define ACE4_SUCCESSFUL_ACCESS_ACE_FLAG 	0x00000010
#define ACE4_FAILED_ACCESS_ACE_FLAG     	0x00000020
#define ACE4_IDENTIFIER_GROUP           	0x00000040

/* ace mask and desired access open */

#define ACE4_READ_DATA				0x00000001
#define ACE4_LIST_DIRECTORY			ACE4_READ_DATA
#define ACE4_WRITE_DATA				0x00000002
#define ACE4_ADD_FILE				ACE4_WRITE_DATA
#define ACE4_APPEND_DATA			0x00000004
#define ACE4_ADD_SUBDIRECTORY			ACE4_APPEND_DATA
#define ACE4_READ_NAMED_ATTRS			0x00000008
#define ACE4_WRITE_NAMED_ATTRS			0x00000010
#define ACE4_EXECUTE				0x00000020
#define ACE4_DELETE_CHILD			0x00000040
#define ACE4_READ_ATTRIBUTES			0x00000080
#define ACE4_WRITE_ATTRIBUTES			0x00000100
#define ACE4_DELETE				0x00010000
#define ACE4_READ_ACL				0x00020000
#define ACE4_WRITE_ACL				0x00040000
#define ACE4_WRITE_OWNER			0x00080000
#define ACE4_SYNCHRONIZE			0x00100000

#endif
/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _SFTP_PROTOCOL_V05_H
#define _SFTP_PROTOCOL_V05_H

#include "acl-ace4.h"
#include "attr-indices.h"

/*
    Definitions as described in:
    https://tools.ietf.org/html/draft-ietf-secsh-filexfer-05
*/

/* attributes valid */

#define SSH_FILEXFER_ATTR_TYPE			0
#define SSH_FILEXFER_ATTR_SIZE 			1 << SSH_FILEXFER_INDEX_SIZE
#define SSH_FILEXFER_ATTR_PERMISSIONS 		1 << SSH_FILEXFER_INDEX_PERMISSIONS
#define SSH_FILEXFER_ATTR_ACCESSTIME 		1 << SSH_FILEXFER_INDEX_ACCESSTIME
#define SSH_FILEXFER_ATTR_CREATETIME 		1 << SSH_FILEXFER_INDEX_CREATETIME
#define SSH_FILEXFER_ATTR_MODIFYTIME 		1 << SSH_FILEXFER_INDEX_MODIFYTIME
#define SSH_FILEXFER_ATTR_ACL 			1 << SSH_FILEXFER_INDEX_ACL
#define SSH_FILEXFER_ATTR_OWNERGROUP 		1 << SSH_FILEXFER_INDEX_OWNERGROUP
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES 	1 << SSH_FILEXFER_INDEX_SUBSECOND_TIMES
#define SSH_FILEXFER_ATTR_BITS			1 << SSH_FILEXFER_INDEX_BITS
#define SSH_FILEXFER_ATTR_EXTENDED	 	1 << SSH_FILEXFER_INDEX_EXTENDED

#define SSH_FILEXFER_STAT_VALUE			( SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_OWNERGROUP | SSH_FILEXFER_ATTR_SUBSECOND_TIMES )

/* attrib bits */

#define SSH_FILEXFER_ATTR_FLAGS_READONLY         0x00000001
#define SSH_FILEXFER_ATTR_FLAGS_SYSTEM           0x00000002
#define SSH_FILEXFER_ATTR_FLAGS_HIDDEN           0x00000004
#define SSH_FILEXFER_ATTR_FLAGS_CASE_INSENSITIVE 0x00000008
#define SSH_FILEXFER_ATTR_FLAGS_ARCHIVE          0x00000010
#define SSH_FILEXFER_ATTR_FLAGS_ENCRYPTED        0x00000020
#define SSH_FILEXFER_ATTR_FLAGS_COMPRESSED       0x00000040
#define SSH_FILEXFER_ATTR_FLAGS_SPARSE           0x00000080
#define SSH_FILEXFER_ATTR_FLAGS_APPEND_ONLY      0x00000100
#define SSH_FILEXFER_ATTR_FLAGS_IMMUTABLE        0x00000200
#define SSH_FILEXFER_ATTR_FLAGS_SYNC             0x00000400

/* error codes */

#define SSH_FX_OK 				0
#define SSH_FX_EOF 				1
#define SSH_FX_NO_SUCH_FILE 			2
#define SSH_FX_PERMISSION_DENIED 		3
#define SSH_FX_FAILURE 				4
#define SSH_FX_BAD_MESSAGE 			5
#define SSH_FX_NO_CONNECTION 			6
#define SSH_FX_CONNECTION_LOST 			7
#define SSH_FX_OP_UNSUPPORTED 			8
#define SSH_FX_INVALID_HANDLE 			9
#define SSH_FX_NO_SUCH_PATH 			10
#define SSH_FX_FILE_ALREADY_EXISTS 		11
#define SSH_FX_WRITE_PROTECT	 		12
#define SSH_FX_NO_MEDIA 			13
#define SSH_FX_NO_SPACE_ON_FILESYSTEM		14
#define SSH_FX_QUOTA_EXCEEDED			15
#define SSH_FX_UNKNOWN_PRINCIPAL		16
#define SSH_FX_LOCK_CONFLICT			17

/* file types */

#define SSH_FILEXFER_TYPE_SOCKET		6
#define SSH_FILEXFER_TYPE_CHAR_DEVICE		7
#define SSH_FILEXFER_TYPE_BLOCK_DEVICE		8
#define SSH_FILEXFER_TYPE_FIFO			9

/* open flags */

#define SSH_FXF_ACCESS_DISPOSITION  		0x00000007
#define SSH_FXF_CREATE_NEW          		0x00000000
#define SSH_FXF_CREATE_TRUNCATE     		0x00000001
#define SSH_FXF_OPEN_EXISTING       		0x00000002
#define SSH_FXF_OPEN_OR_CREATE      		0x00000003
#define SSH_FXF_TRUNCATE_EXISTING   		0x00000004
#define SSH_FXF_APPEND_DATA         		0x00000008
#define SSH_FXF_APPEND_DATA_ATOMIC      	0x00000010
#define SSH_FXF_TEXT_MODE               	0x00000020
#define SSH_FXF_READ_LOCK              		0x00000040
#define SSH_FXF_WRITE_LOCK             		0x00000080
#define SSH_FXF_DELETE_LOCK            		0x00000100

/* rename flags */

#define SSH_FXF_RENAME_OVERWRITE		0x00000001
#define SSH_FXF_RENAME_ATOMIC			0x00000002
#define SSH_FXF_RENAME_NATIVE			0x00000004

#endif

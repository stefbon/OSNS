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

#ifndef _SFTP_PROTOCOL_V03_H
#define _SFTP_PROTOCOL_V03_H

#include "attr-indices.h"

/*
    Definitions as described in:
    https://tools.ietf.org/html/draft-ietf-secsh-filexfer-02
*/

/* attributes valid */

#define SSH_FILEXFER_ATTR_TYPE			0
#define SSH_FILEXFER_ATTR_SIZE 			1 << SSH_FILEXFER_INDEX_SIZE
#define SSH_FILEXFER_ATTR_UIDGID		1 << SSH_FILEXFER_INDEX_UIDGID
#define SSH_FILEXFER_ATTR_PERMISSIONS 		1 << SSH_FILEXFER_INDEX_PERMISSIONS
#define SSH_FILEXFER_ATTR_ACMODTIME 		1 << SSH_FILEXFER_INDEX_ACMODTIME
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES 	1 << SSH_FILEXFER_INDEX_SUBSECOND_TIMES
#define SSH_FILEXFER_ATTR_EXTENDED	 	1 << SSH_FILEXFER_INDEX_EXTENDED

#define SSH_FILEXFER_STAT_VALUE			( SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME )


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

/* open pflags */

#define SSH_FXF_READ     			0x00000001
#define SSH_FXF_WRITE       			0x00000002
#define SSH_FXF_APPEND      			0x00000004
#define SSH_FXF_CREAT   			0x00000008
#define SSH_FXF_TRUNC         			0x00000010
#define SSH_FXF_EXCL				0x00000020

#endif

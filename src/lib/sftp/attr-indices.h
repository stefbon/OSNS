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

#ifndef LIB_SFTP_ATTR_INDICES_H
#define LIB_SFTP_ATTR_INDICES_H

#define SSH_FILEXFER_INDEX_TYPE			32
#define SSH_FILEXFER_INDEX_SIZE			0
#define SSH_FILEXFER_INDEX_UIDGID		1
#define SSH_FILEXFER_INDEX_PERMISSIONS		2
#define SSH_FILEXFER_INDEX_ACMODTIME		3
#define SSH_FILEXFER_INDEX_ACCESSTIME		3
#define SSH_FILEXFER_INDEX_CREATETIME		4
#define SSH_FILEXFER_INDEX_MODIFYTIME		5
#define SSH_FILEXFER_INDEX_ACL			6
#define SSH_FILEXFER_INDEX_OWNERGROUP		7
#define SSH_FILEXFER_INDEX_SUBSECOND_TIMES	8
#define SSH_FILEXFER_INDEX_BITS			9
#define SSH_FILEXFER_INDEX_ALLOCATION_SIZE	10
#define SSH_FILEXFER_INDEX_TEXT_HINT		11
#define SSH_FILEXFER_INDEX_MIME_TYPE		12
#define SSH_FILEXFER_INDEX_LINK_COUNT		13
#define SSH_FILEXFER_INDEX_UNTRANSLATED_NAME	14
#define SSH_FILEXFER_INDEX_CTIME		15
#define SSH_FILEXFER_INDEX_CHANGETIME		SSH_FILEXFER_INDEX_CTIME

#define SSH_FILEXFER_INDEX_EXTENDED		31

#define SSH_FILEXFER_INDEX_NSEC_ATIME		33
#define SSH_FILEXFER_INDEX_NSEC_MTIME		34
#define SSH_FILEXFER_INDEX_NSEC_BTIME		35
#define SSH_FILEXFER_INDEX_NSEC_CTIME		36

#endif
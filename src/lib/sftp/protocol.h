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

#ifndef _SFTP_PROTOCOL_H
#define _SFTP_PROTOCOL_H

#define SSH_FXP_INIT 				1
#define SSH_FXP_VERSION 			2
#define SSH_FXP_OPEN 				3
#define SSH_FXP_CLOSE 				4
#define SSH_FXP_READ 				5
#define SSH_FXP_WRITE 				6
#define SSH_FXP_LSTAT 				7
#define SSH_FXP_FSTAT 				8
#define SSH_FXP_SETSTAT 			9
#define SSH_FXP_FSETSTAT 			10
#define SSH_FXP_OPENDIR 			11
#define SSH_FXP_READDIR 			12
#define SSH_FXP_REMOVE 				13
#define SSH_FXP_MKDIR 				14
#define SSH_FXP_RMDIR 				15
#define SSH_FXP_REALPATH 			16
#define SSH_FXP_STAT 				17
#define SSH_FXP_RENAME 				18
#define SSH_FXP_READLINK 			19
#define SSH_FXP_SYMLINK 			20
#define SSH_FXP_LINK 				21
#define SSH_FXP_BLOCK 				22
#define SSH_FXP_UNBLOCK 			23

#define SSH_FXP_STATUS 				101
#define SSH_FXP_HANDLE 				102
#define SSH_FXP_DATA 				103
#define SSH_FXP_NAME 				104
#define SSH_FXP_ATTRS 				105

#define SSH_FXP_EXTENDED 			200
#define SSH_FXP_EXTENDED_REPLY 			201

#define SSH_FXP_MAPPING_MIN			210
#define SSH_FXP_MAPPING_MAX			255

#define SFTP_HANDLE_MAXSIZE			255

#endif

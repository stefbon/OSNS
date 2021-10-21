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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <pwd.h>

#include "main.h"
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "mountinfo.h"

#include "misc.h"
#include "osns_sftp_subsystem.h"
#include "protocol.h"

unsigned int get_attrib_extensions(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *count)
{
    /* extensions to the attrib structure
	not supported */

    *count=0;
    return 0;
}

    /* TODO:
	extension which gives the extension a (free) number 210-255
	which is much more efficient than using the names
	something like "mapextension@bononline.nl"

	client sends:
	- byte 			SSH_FXP_EXTENDED
	- uint32		id
	- string		"mapextension@bononline.nl"
	- string		"name of extension to map"

	server replies:
	- byte			SSH_FXP_EXTENDED_REPLY
	- uint32		id
	- byte			number to use

	or when error:
	- byte			SSH_FXP_STATUS
	- uint32		id
	- uint32		error code
	- string		error string
	like:
	- SSH_FX_OP_UNSUPPORTED: extension not supported
	- SSH_FX_INVALID_PARAMETER: name of extension to map not reckognized or extension already mapped
	- SSH_FX_FAILURE: failed like too many extensions to map


*/

unsigned int get_sftp_extensions(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *count)
{

    logoutput("get_sftp_extensions");

    /* names of supported extensions */
    return 0;

}

unsigned int get_supported_attribute_flags(struct sftp_subsystem_s *session)
{
    return SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_OWNERGROUP | SSH_FILEXFER_ATTR_SUBSECOND_TIMES | SSH_FILEXFER_ATTR_CTIME;
}

unsigned int get_supported_attribute_bits(struct sftp_subsystem_s *session)
{
    return 0;
}

unsigned int get_supported_open_flags(struct sftp_subsystem_s *session)
{
    /*
	sftp open flags
	TODO:
	- SSH_FXF_BLOCK_READ/WRITE etc. */

    return SSH_FXF_ACCESS_DISPOSITION | SSH_FXF_APPEND_DATA;
}

unsigned int get_supported_acl_access_mask(struct sftp_subsystem_s *session)
{
    /*
	sftp write acl mask */

    return 0;
}

unsigned int get_supported_max_readsize(struct sftp_subsystem_s *session)
{
    /*
	max read size */

    return 8192;

}

unsigned int get_supported_open_block_flags(struct sftp_subsystem_s *session)
{
    /*
	16-bit masks specifying which combinations of blocking flags are supported with open
	TODO: find out how this works */

    return 1;
}

unsigned int get_supported_block_flags(struct sftp_subsystem_s *session)
{
    /*
	16-bit masks specifying which combinations of blocking flags are supported
	TODO: find out how this works */

    /* translate the combination of block flags like:
	- BLOCK_READ			1000000		->	1
	- BLOCK_WRITE		       10000000		->     10
	- BLOCK_DELETE		      100000000		->    100
	- BLOCK_ADVISORY	     1000000000		->   1000
	*/

    return 1;
}

unsigned int get_supported_acl_cap(struct sftp_subsystem_s *session)
{
    return 0;
}

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
#include "init.h"
#include "extensions.h"

unsigned int get_supported_attr_valid_mask(struct sftp_subsystem_s *sftp)
{
    /* send the valid the server can read == the valid the client can write */

    return (sftp->attrctx.r_valid.flags | sftp->attrctx.r_valid.mask);
}

unsigned int get_supported_attr_attr_bits(struct sftp_subsystem_s *sftp)
{
    return 0; /* ignored */
}

unsigned int get_supported_open_flags(struct sftp_subsystem_s *sftp)
{
    /* sftp open flags */
    return (SSH_FXF_ACCESS_DISPOSITION | SSH_FXF_APPEND_DATA | SSH_FXF_APPEND_DATA_ATOMIC | SSH_FXF_BLOCK_READ | SSH_FXF_BLOCK_WRITE | SSH_FXF_BLOCK_DELETE);
}

unsigned int get_supported_open_access(struct sftp_subsystem_s *sftp)
{
    /* sftp open access bits */
    return (ACE4_READ_DATA | ACE4_READ_ATTRIBUTES | ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_APPEND_DATA | ACE4_DELETE);
}

unsigned int get_supported_max_readsize(struct sftp_subsystem_s *session)
{
    /*
	max read size */

    return 8192;

}

    /*
	16-bit masks specifying which combinations of blocking flags are supported
	TODO: find out how this works */

    /* translate the combination of block flags like:
	- BLOCK_READ			1000000		->	1
	- BLOCK_WRITE		       10000000		->     10
	- BLOCK_DELETE		      100000000		->    100
	- BLOCK_ADVISORY	     1000000000		->   1000
	*/


unsigned int get_supported_open_block_flags(struct sftp_subsystem_s *session)
{
    /* 16-bit masks specifying which combinations of blocking flags are supported with open
	NOTE: these flags indicate that there is no other client gets access using this flags: they are blocking flags
	so when you want to write to a file: set BLOCK_READ means no other client/process may read in this byte range
	NOTE: an exclusive lock is blcoking every other access BLOCK_READ | BLOCK_WRITE | BLOCK_DELETE */

    /* 	no locking					->		0000
	READ						->		0001
	WRITE						->		0010
	DELETE						->		0100
	ADVISORY					->		1000

    just read: block writers and delete			->		0010 | 0100 = 0110 = 6
    just want write access: block delete, read, write	->		0100 | 0010 | 0001 = 7

    read & write block: 2 and 4						bit 8

    no block are always supported: 1 shifted 0  is 1	->		0001

    result:								0111
    */

    return 1;
}

unsigned int get_supported_block_flags(struct sftp_subsystem_s *sftp)
{

    return 1;
}

unsigned int get_sftp_attrib_extensions(struct sftp_subsystem_s *sftp, struct sftp_init_extensions_s *init)
{
    /* extensions to the attr structure
	not supported */

    init->count=0;
    return 0;
}

unsigned int get_sftp_protocol_extensions(struct sftp_subsystem_s *sftp, struct sftp_init_extensions_s *init)
{
    unsigned int pos=0;

    /* names of ALL available extensions */

    struct sftp_protocol_extension_s *ext=get_next_sftp_protocol_extension(NULL, 0);

    while (ext) {
	unsigned int len=0;

	logoutput("get_sftp_protocol_extensions: add %s", ext->name);

	if (init->buffer) {

	    len=write_ssh_string(init->buffer, init->len, 'c', (void *) ext->name);
	    init->buffer += len;
	    init->len -= len;

	} else {

	    len = 4 + strlen(ext->name);

	}

	pos += len;
	init->count++;
	ext=get_next_sftp_protocol_extension(ext, 0);

    }

    return pos;

}


unsigned int get_supported_acl_cap(struct sftp_subsystem_s *session)
{
    return 0;
}

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

#include "main.h"
#include "log.h"
#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/protocol-v06.h"
#include "sftp/attr-context.h"
#include "common.h"
#include "init.h"

#include "sftp/attr-read.h"
#include "sftp/attr-write.h"

void set_sftp_attr_version(struct sftp_client_s *sftp)
{
    set_sftp_attr_context(&sftp->attrctx);
}

static unsigned int get_sftp_client_flags(struct attr_context_s *ctx, const char *what)
{
    return 0;
}

static unsigned char get_sftp_client_protocol_version(struct attr_context_s *ctx)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *)((char *) ctx - offsetof(struct sftp_client_s, attrctx));
    return get_sftp_protocol_version(sftp);
}

void init_sftp_client_attr_context(struct sftp_client_s *sftp)
{

    init_attr_context(&sftp->attrctx, ATTR_CONTEXT_FLAG_CLIENT, NULL, sftp->mapping);
    sftp->attrctx.get_sftp_flags=get_sftp_client_flags;
    sftp->attrctx.get_sftp_protocol_version=get_sftp_client_protocol_version;
}


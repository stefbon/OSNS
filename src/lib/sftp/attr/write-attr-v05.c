/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/protocol-v06.h"
#include "sftp/attr-context.h"

#include "write-attr-generic.h"
#include "write-attr-v03.h"
#include "write-attr-v04.h"
#include "write-attr-v05.h"

void write_attributes_v05(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct sftp_attr_s *attr)
{
    write_attributes_v04(ctx, buffer, r, attr);
}

unsigned int write_attributes_len_v05(struct attr_context_s *ctx, struct rw_attr_result_s *r ,struct sftp_attr_s *attr)
{
    return write_attributes_len_v04(ctx, r, attr);
}

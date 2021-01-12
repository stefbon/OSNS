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

#include "common-protocol.h"
#include "common.h"

#include "attr/read-attr-v03.h"
#include "attr/read-attr-v04.h"
#include "attr/read-attr-v05.h"
#include "attr/read-attr-v06.h"

#include "attr/write-attr-v03.h"
#include "attr/write-attr-v04.h"
#include "attr/write-attr-v05.h"
#include "attr/write-attr-v06.h"

#include "attr/read-name-v03.h"

void set_sftp_attr(struct sftp_client_s *sftp)
{

    struct sftp_attr_ops_s *ops=&sftp->attr_ops;

    if (sftp->server_version<=3) {

	ops->read_attributes			= read_attributes_v03;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attr_nameresponse_v03;
	ops->read_sftp_features			= read_sftp_features_v03;
	ops->get_attribute_mask			= get_attribute_mask_v03;
	ops->write_attributes			= write_attributes_v03;
	ops->write_attributes_len		= write_attributes_len_v03;

    } else if (sftp->server_version==4) {

	ops->read_attributes			= read_attributes_v04;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attributes_v04;
	ops->read_sftp_features			= read_sftp_features_v04;
	ops->get_attribute_mask			= get_attribute_mask_v04;
	ops->write_attributes			= write_attributes_v04;
	ops->write_attributes_len		= write_attributes_len_v04;

    } else if (sftp->server_version==5) {

	ops->read_attributes			= read_attributes_v05;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attributes_v05;
	ops->read_sftp_features			= read_sftp_features_v05;
	ops->get_attribute_mask			= get_attribute_mask_v05;
	ops->write_attributes			= write_attributes_v05;
	ops->write_attributes_len		= write_attributes_len_v05;

    } else if (sftp->server_version==6) {

	ops->read_attributes			= read_attributes_v06;
	ops->read_name_response			= read_name_nameresponse_v03;
	ops->read_attr_response			= read_attributes_v06;
	ops->read_sftp_features			= read_sftp_features_v06;
	ops->get_attribute_mask			= get_attribute_mask_v05;
	ops->write_attributes			= write_attributes_v06;
	ops->write_attributes_len		= write_attributes_len_v06;

    } else {

	logoutput_warning("set_sftp_attr: error server version %i not supported", sftp->server_version);

    }

}

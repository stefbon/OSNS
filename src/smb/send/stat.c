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

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#include "log.h"
#include "main.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "interface/smb-signal.h"
#include "interface/smb.h"
#include "interface/smb-wait-response.h"

extern struct smb2_context *get_smb2_context_smb_interface(struct context_interface_s *interface);

static void _smb_stat_cb(struct smb2_context *smb2, int status, void *command_data, void *private_data)
{
    struct smb_data_s *data=(struct smb_data_s *) private_data;
    struct smb2_stat_64 *st=NULL;
    struct smb_request_s *r=get_smb_request(data->interface, data->id, NULL);

    if (status) {

	status=-status;

	logoutput_warning("_smb_stat_cb: failed to get file stat %i - %s - %s", status, strerror(status), smb2_get_error(smb2));

	if (r) {

	    r->error=status;
	    signal_smb_received_id_error(data->interface, r, NULL);

	}

	remove_list_element(&data->list);
	free(data);
	return;

    }

    if (r) {

	/* give smb data with result to smb request */

	r->data=data;
	signal_smb_received_id(data->interface, r);
	return;

    }

    /* no smb request found ... timedout ??*/

    logoutput_warning("_smb_stat_cb: original smb request not found (timedout..?)");
    remove_list_element(&data->list);
    free(data);

}

int send_smb_stat_ctx(struct context_interface_s *interface, struct smb_request_s *smb_r, char*path, struct smb_data_s *data)
{
    struct smb2_context *smb2=get_smb2_context_smb_interface(interface);
    return smb2_stat_async(smb2, path, (struct smb2_stat_64 *) data->buffer, _smb_stat_cb, (void *) data);
}

unsigned int get_size_buffer_smb_stat()
{
    return sizeof(struct smb2_stat_64);
}

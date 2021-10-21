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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "interface/smb-signal.h"
#include "interface/smb.h"
#include "interface/smb-wait-response.h"

#include "smb/send/stat.h"

static const char *rootpath="/.";

/* GETATTR */

void _fs_smb_getattr(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    struct context_interface_s *interface=&context->interface;
    struct smb_request_s smb_r;
    unsigned int error=EIO;
    struct smb_data_s *data=NULL;
    unsigned int size=get_size_buffer_smb_stat();

    data=malloc(sizeof(struct smb_data_s) + size);

    if (data==NULL) {

	error=ENOMEM;
	goto out;

    }

    memset(data, 0, size);
    data->interface=interface;
    init_list_element(&data->list, NULL);
    data->id=0;
    data->size=size;

    logoutput("_fs_smb_getattr: %li %s", inode->st.st_ino, pathinfo->path);

    init_smb_request(&smb_r, interface, f_request);

    if (send_smb_stat_ctx(interface, &smb_r, pathinfo->path, data)>0) {
	struct timespec timeout;

	get_smb_request_timeout_ctx(interface, &timeout);
	data->id=get_smb_unique_id(interface);
	smb_r.id=data->id;

	if (wait_smb_response_ctx(interface, &smb_r, &timeout)==1) {

	    /* fill inode stat */

	    fill_inode_attr_smb(interface, &inode->st, data->buffer);
	    _fs_common_getattr(get_root_context(context), f_request, inode);
	    get_current_time(&inode->stim);
	    unset_fuse_request_flags_cb(f_request);
	    free(data);
	    return;

	}

	/* put smb data on special list */

	add_smb_list_pending_requests_ctx(interface, &data->list);

    }

    error=(smb_r.error) ? smb_r.error : EPROTO;

    out:

    logoutput("_fs_smb_getattr: error %i (%s)", error, strerror(error));
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

void _fs_smb_getattr_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    _fs_common_getattr(get_root_context(context), f_request, inode);
}

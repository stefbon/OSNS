/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include <sys/syscall.h>
#include <sys/poll.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "users.h"

#include "smb.h"
#include "smb-signal.h"
#include "smb-utils.h"

extern struct context_interface_s *get_parent_interface(struct context_interface_s *i);
extern struct passwd *get_workspace_user_pwd(struct context_interface_s *i);
extern struct beventloop_s *get_workspace_eventloop(struct context_interface_s *i);

/* enumerate smb shares */

struct _enum_smb_share_s {
    struct context_interface_s 		*interface;
    struct smb_signal_s 		*signal;
    void				*ptr;
    void				(* cb)(struct context_interface_s *i, char *name, unsigned int type, unsigned int flags, void *ptr);
    unsigned char			ready;
};

static void default_smb2_cb(struct smb2_context *smb2, int status, void *command_data, void *private_data)
{
    struct _enum_smb_share_s *es=NULL;
    struct srvsvc_netshareenumall_rep *rep = NULL;
    struct smb_signal_s *signal=NULL;

    if (status) {

	logoutput_warning("default_smb2_cb: failed to enumerate shares (%s) %s", strerror(-status), smb2_get_error(smb2));
	(* es->cb)(es->interface, NULL, -status, SMB_SHARE_FLAG_ERROR, es->ptr);
	return;

    }

    es=(struct _enum_smb_share_s *) private_data;
    rep = (struct srvsvc_netshareenumall_rep *) command_data;
    signal=es->signal;

    for (unsigned int i = 0; i < rep->ctr->ctr1.count; i++) {
	unsigned int type=0;

	logoutput("default_smb2_cb: %i %-20s %-20s", rep->ctr->ctr1.array[i].type, rep->ctr->ctr1.array[i].name, rep->ctr->ctr1.array[i].comment);

        switch (rep->ctr->ctr1.array[i].type & 3) {

	    case SHARE_TYPE_DISKTREE:

		type=SMB_SHARE_TYPE_DISKTREE;
		break;

	    case SHARE_TYPE_PRINTQ:

		type=SMB_SHARE_TYPE_PRINTQ;
		break;

	    case SHARE_TYPE_DEVICE:

		type=SMB_SHARE_TYPE_DEVICE;
		break;

	    case SHARE_TYPE_IPC:

		type=SMB_SHARE_TYPE_IPC;
		break;

	}

	if (type>0) {
	    unsigned int flags=0;

	    flags |= (rep->ctr->ctr1.array[i].type & SHARE_TYPE_TEMPORARY) ? SMB_SHARE_FLAG_TEMPORARY : 0;
	    flags |= (rep->ctr->ctr1.array[i].type & SHARE_TYPE_HIDDEN) ? SMB_SHARE_FLAG_HIDDEN : 0;

	    (* es->cb)(es->interface, rep->ctr->ctr1.array[i].name, type, flags, es->ptr);

	} else {

	    logoutput_warning("default_smb2_cb: type %i not reckognized", rep->ctr->ctr1.array[i].type);

	}

    }

    smb2_free_data(smb2, rep);

    smb_signal_lock(signal);
    es->ready=1;
    smb_signal_broadcast(signal);
    smb_signal_unlock(signal);

}

int smb_share_enum_async_ctx(struct context_interface_s *interface, void (* cb)(struct context_interface_s *interface, char *name, unsigned int type, unsigned int flags, void *ptr), void *ptr)
{
    struct smb_share_s *share=(struct smb_share_s *) (* interface->get_interface_buffer)(interface);
    struct smb2_context *smb2=(struct smb2_context *) share->ptr;
    struct _enum_smb_share_s enum_share;
    struct smb_signal_s *signal=get_smb_signal_ctx(interface);

    if (signal==NULL) return -1;

    memset(&enum_share, 0, sizeof(struct _enum_smb_share_s));

    enum_share.interface=interface;
    enum_share.ptr=ptr;
    enum_share.signal=signal;
    enum_share.cb=cb;
    enum_share.ready=0;

    if (smb2_share_enum_async(smb2, default_smb2_cb, (void *) &enum_share)==0) {
	struct timespec timeout;
	struct timespec expire;

	get_smb_request_timeout_ctx(interface, &timeout);
	get_expire_time(&expire, &timeout);

	smb_signal_lock(signal);

	while (enum_share.ready==0) {

	    int error=smb_signal_condtimedwait(signal, &expire);

	    if (enum_share.ready) {

		break;

	    } else if (error>0) {

		/* some error (very possible: timeout) */

		logoutput_warning("smb_share_enum_async_ctx: error %i waiting for completion (%s)", error, strerror(error));
		break;

	    }

	}

	smb_signal_unlock(signal);

    }

    return ((enum_share.ready) ? 0 : -1);

}


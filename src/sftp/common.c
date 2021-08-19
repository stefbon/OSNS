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

#include "main.h"
#include "log.h"
#include "misc.h"
#include "threads.h"
#include "error.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/attr-context.h"
#include "request-hash.h"

#include "time.h"
#include "usermapping.h"
#include "extensions.h"
#include "init.h"
#include "context.h"
#include "recv.h"
#include "attr.h"

uint32_t get_sftp_request_id(struct sftp_client_s *sftp)
{
    struct sftp_sendhash_s *sendhash=&sftp->sendhash;
    uint32_t id=0;

    pthread_mutex_lock(&sendhash->mutex);
    id=sendhash->id;
    sendhash->id++;
    pthread_mutex_unlock(&sendhash->mutex);

    return id;
}

void get_sftp_request_timeout(struct sftp_client_s *sftp, struct timespec *timeout)
{
    /* make this configurable */
    timeout->tv_sec=4;
    timeout->tv_nsec=0;
}

struct sftp_client_s *create_sftp_client(struct generic_error_s *error)
{
    struct sftp_client_s *sftp=malloc(sizeof (struct sftp_client_s));

    if (sftp) {

	memset(sftp, 0, sizeof(struct sftp_client_s));
	sftp->flags |= SFTP_CLIENT_FLAG_ALLOC;

    } else {

	set_generic_error_system(error, ENOMEM, __PRETTY_FUNCTION__);

    }

    return sftp;
}

int init_sftp_client(struct sftp_client_s *sftp, uid_t uid)
{

    sftp->context.unique=0;
    sftp->context.ctx=NULL;
    sftp->context.conn=NULL;
    init_sftp_default_context(sftp);

    sftp->signal.flags=0;
    sftp->signal.signal=NULL;
    sftp->signal.seq=0;
    sftp->signal.seqset.tv_sec=0;
    sftp->signal.seqset.tv_nsec=0;
    sftp->signal.seqtype=0;
    init_generic_error(&sftp->signal.error);

    pthread_mutex_init(&sftp->mutex, NULL);
    sftp->refcount=0;

    sftp->protocol.version=0;

    pthread_mutex_init(&sftp->sendhash.mutex, NULL);
    sftp->sendhash.id=0;

    init_list_header(&sftp->pending, SIMPLE_LIST_TYPE_EMPTY, NULL);

    sftp->send_ops=NULL;
    sftp->recv_ops=NULL;;

    init_sftp_receive(sftp);
    init_sftp_timecorrection(sftp);
    init_sftp_extensions(sftp);
    init_sftp_sendhash();
    init_sftp_usermapping(sftp, uid);
    init_sftp_client_attr_context(sftp);

    /* here link the attr_context->get_remote_uid/get_local_uid... functions 
	to the function in usermapping */

    init_readattr_generic();
    init_writeattr_generic();

    return 0;
}

void clear_sftp_client(struct sftp_client_s *sftp)
{

    clear_sftp_extensions(sftp);
    free_sftp_usermapping(sftp);

    pthread_mutex_destroy(&sftp->sendhash.mutex);

    clear_readattr_generic(0);
    clear_writeattr_generic(0);
}

void free_sftp_client(struct sftp_client_s **p_sftp)
{
    struct sftp_client_s *sftp=*p_sftp;

    if (sftp->flags & SFTP_CLIENT_FLAG_ALLOC) {

	free(sftp);
	p_sftp=NULL;

    }

}

int start_init_sftp_client(struct sftp_client_s *sftp)
{
    struct sftp_request_s sftp_r;
    int protocol=0;
    struct context_interface_s *interface=NULL;

    if (sftp==NULL) {

	logoutput("start_init_sftp_client: sftp not defined");
	return -1;

    }

    interface=(struct context_interface_s *) ((char *) sftp - offsetof(struct context_interface_s, buffer));

    set_sftp_protocol_version(sftp, 6); /* start with -> 6 <- */
    set_sftp_protocol(sftp);
    init_sftp_extensions(sftp);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=(uint32_t) -1; /* use a custom id */
    sftp_r.call.init.version=get_sftp_protocol_version(sftp);
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;
    sftp_r.interface=interface;

    logoutput("start_init_sftp_client");

    /* start the sftp init negotiation */

    if ((* sftp->send_ops->init)(sftp, &sftp_r)>0) {
	struct timespec timeout;
	unsigned int error=0;
	struct sftp_signal_s *signal=&sftp->signal;
	struct list_element_s *list=NULL;

	get_sftp_request_timeout(sftp, &timeout);

	if (wait_sftp_response(sftp, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_VERSION) {

		if (process_sftp_version(sftp, reply)==0) {

		    logoutput("start_init_sftp_client: server sftp version processed");

		} else {

		    logoutput("start_init_sftp_client: error processing server sftp init");
		    goto error;

		}

	    } else {

		logoutput("start_init_sftp_client: no init response from server (received %i)", reply->type);
		goto error;

	    }

	} else {

	    logoutput("start_init_sftp_client: no init response received");
	    goto error;

	}

    } else {

	logoutput("start_init_sftp_client: error sending sftp init");
	goto error;

    }

    protocol=set_sftp_protocol(sftp);

    if (protocol==-1) {

	logoutput("start_init_sftp_client: error setting protocol version");
	goto error;

    } else {

	logoutput("start_init_sftp_client: sftp protocol version set to %i", protocol);

    }

    enable_timecorrection(sftp);

    if (set_sftp_usermapping(sftp)==0) {

	logoutput("start_init_sftp_client: initialized sftp usermapping");

    } else {

	logoutput("start_init_sftp_client: failed initializing sftp usermapping");
	goto error;

    }

    complete_sftp_protocolextensions(sftp, NULL);
    return 0;

    error:
    return -1;

}

unsigned int get_sftp_features(struct sftp_client_s *sftp)
{
    struct sftp_supported_s *supported=&sftp->supported;
    return (supported->attr_supported);
}

unsigned int get_sftp_buffer_size()
{
    return sizeof(struct sftp_client_s);
}

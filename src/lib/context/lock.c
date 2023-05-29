/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "libosns-basic-system-headers.h"

#include <sys/fsuid.h>

#include "libosns-log.h"
#include "libosns-threads.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse-public.h"

#include "context.h"
#include "next.h"
#include "lock.h"

/* locking flags */

/* how */
#define _CTX_LOCK_FLAG_READ				1
#define _CTX_LOCK_FLAG_WRITE				2

/* what */
#define _CTX_LOCK_FLAG_ROOT				4
#define _CTX_LOCK_FLAG_PARENT				8
#define _CTX_LOCK_FLAG_SELF				16

/* lock or unlock */
#define _CTX_LOCK_FLAG_LOCK				32

static int get_write_access_ctx(struct service_context_lock_s *servicelock, struct list_lock_s *lock, const char *what)
{
    unsigned int *p_lockflags=NULL;

    logoutput_debug("get_write_access_ctx: value %i", lock->value);

    if (strcmp(what, "w")==0) {

	p_lockflags=&servicelock->wlockflags;

    } else if (strcmp(what, "p")==0) {

	p_lockflags=&servicelock->plockflags;

    } else if (strcmp(what, "c")==0) {

	p_lockflags=&servicelock->clockflags;

    }

    if (lock->value & SIMPLE_LOCK_WRITE) {

	if (lock->threadidw==pthread_self()) {

	    /* already write permissions */
	    return 1;

	} else {

	    if ((lock->value & SIMPLE_LOCK_PREWRITE)==0) {

		lock->value |= SIMPLE_LOCK_PREWRITE;
		lock->threadidpw=pthread_self();
		*p_lockflags |= SIMPLE_LOCK_PREWRITE;

	    }

	    return 0;

	}

    } else if (lock->value & SIMPLE_LOCK_PREWRITE) {

	if (lock->threadidpw==pthread_self()) {

	    /* pre write owned by this thread */

	    if (lock->value <= (SIMPLE_LOCK_PREWRITE | SIMPLE_LOCK_WRITE)) {

		/* no readers */

		lock->value |= SIMPLE_LOCK_WRITE;
		lock->value &= ~ SIMPLE_LOCK_PREWRITE;
		lock->threadidw=lock->threadidpw;
		lock->threadidpw=0;
		*p_lockflags |= SIMPLE_LOCK_WRITE;
		*p_lockflags &= ~ SIMPLE_LOCK_PREWRITE;

		return 1;

	    } else {

		/* still readers */

		return 0;

	    }

	} else {

	    /* reservation on write owned by another thread */

	    return 0;

	}

    } else if (lock->value==0) {

	/* no reader, no writer: take it */

	lock->value |= SIMPLE_LOCK_WRITE;
	lock->threadidw=pthread_self();
	*p_lockflags |= SIMPLE_LOCK_WRITE;

	return 1;

    } else {

	/* there are readers: make a reservation of the write lock */

	lock->value |= SIMPLE_LOCK_PREWRITE;
	lock->threadidpw=pthread_self();
	*p_lockflags |= SIMPLE_LOCK_PREWRITE;

	return 1;

    }

    return 0;

}

static int get_read_access_ctx(struct service_context_lock_s *servicelock, struct list_lock_s *lock, const char *what)
{
    unsigned int *p_lockflags=NULL;

    logoutput_debug("get_read_access_ctx: value %i", lock->value);

    if (strcmp(what, "w")==0) {

	p_lockflags=&servicelock->wlockflags;

    } else if (strcmp(what, "p")==0) {

	p_lockflags=&servicelock->plockflags;

    } else if (strcmp(what, "c")==0) {

	p_lockflags=&servicelock->clockflags;

    }

    if ((lock->value & (SIMPLE_LOCK_PREWRITE | SIMPLE_LOCK_WRITE))==0) {

	lock->value += SIMPLE_LOCK_READ;
	*p_lockflags=SIMPLE_LOCK_READ;
	return 1;

    }

    return 0;

}

static void release_access_ctx(struct service_context_lock_s *servicelock, struct list_lock_s *lock, const char *what)
{
    unsigned int *p_lockflags=NULL;
    unsigned int lockflag=0;

    if (strcmp(what, "w")==0) {

	p_lockflags=&servicelock->wlockflags;

    } else if (strcmp(what, "p")==0) {

	p_lockflags=&servicelock->plockflags;

    } else if (strcmp(what, "c")==0) {

	p_lockflags=&servicelock->clockflags;

    }

    lockflag=*p_lockflags;

    if (lockflag & SIMPLE_LOCK_READ) {

	if (lock->value >= SIMPLE_LOCK_READ) lock->value -= SIMPLE_LOCK_READ;
	lockflag &= ~SIMPLE_LOCK_READ;

    }

    if (lockflag & SIMPLE_LOCK_PREWRITE) {

	if (lock->threadidpw==pthread_self()) {

	    lock->value &= ~SIMPLE_LOCK_PREWRITE;
	    lockflag &= ~SIMPLE_LOCK_PREWRITE;

	}

    }

    if (lockflag & SIMPLE_LOCK_WRITE) {

	if (lock->threadidw==pthread_self()) {

	    lock->value &= ~SIMPLE_LOCK_WRITE;
	    lockflag &= ~SIMPLE_LOCK_WRITE;

	}

    }

    *p_lockflags = lockflag;

}

static int check_lockflags_conflict(struct service_context_lock_s *servicelock)
{
    int result=-1;

    logoutput_debug("check_lockflags_conflict: flag rw %i what %i lock %i", servicelock->flags & (_CTX_LOCK_FLAG_WRITE | _CTX_LOCK_FLAG_READ), servicelock->flags & (_CTX_LOCK_FLAG_ROOT | _CTX_LOCK_FLAG_PARENT | _CTX_LOCK_FLAG_SELF), servicelock->flags & _CTX_LOCK_FLAG_LOCK);

    if (servicelock->flags & _CTX_LOCK_FLAG_ROOT) {
	struct service_context_s *root=servicelock->root;
	struct list_lock_s *wlock=&root->service.workspace.header.lock;

	if (servicelock->flags & _CTX_LOCK_FLAG_LOCK) {

	    /* to add or delete a list element from the workspace/global list required is:
		- write access to workspace list
		- write access to the ctx self */

	    if (servicelock->flags & _CTX_LOCK_FLAG_WRITE) {

		result=get_write_access_ctx(servicelock, wlock, "w");

	    } else if (servicelock->flags & _CTX_LOCK_FLAG_READ) {

		result=get_read_access_ctx(servicelock, wlock, "w");

	    }

	    if (result==0 || result==-1) return result;
	    servicelock->flags &= ~ _CTX_LOCK_FLAG_LOCK;

	} else {

	    /* unlock */

	    release_access_ctx(servicelock, wlock, "w");

	}

    }

    if (servicelock->flags & _CTX_LOCK_FLAG_PARENT) {
	struct service_context_s *pctx=servicelock->pctx;
	struct list_lock_s *plock=NULL;

	if (pctx->type==SERVICE_CTX_TYPE_WORKSPACE) {

	    plock=&pctx->service.workspace.header.lock;

	} else if (pctx->type==SERVICE_CTX_TYPE_BROWSE) {

	    plock=&pctx->service.browse.header.lock;

	} else {

	    return -1;

	}

	if (servicelock->flags & _CTX_LOCK_FLAG_LOCK) {

	    /* to add or delete a list element from the workspace/global list required is:
		- write access to parent list
		- write access to the ctx self */

	    if (servicelock->flags & _CTX_LOCK_FLAG_WRITE) {

		result=get_write_access_ctx(servicelock, plock, "p");

	    } else if (servicelock->flags & _CTX_LOCK_FLAG_READ) {

		result=get_read_access_ctx(servicelock, plock, "p");

	    }

	    if (result==0 || result==-1) return result;
	    servicelock->flags &= ~ _CTX_LOCK_FLAG_LOCK;

	} else {

	    /* unlock */

	    release_access_ctx(servicelock, plock, "p");

	}

    }

    if (servicelock->flags & _CTX_LOCK_FLAG_SELF) {
	struct service_context_s *ctx=servicelock->ctx;
	struct list_lock_s *clock=NULL;

	if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	    clock=&ctx->service.browse.clist.lock;

	} else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    clock=&ctx->service.filesystem.clist.lock;

	} else {

	    return -1;

	}

	if (servicelock->flags & _CTX_LOCK_FLAG_LOCK) {

	    if (servicelock->flags & _CTX_LOCK_FLAG_WRITE) {

		result=get_write_access_ctx(servicelock, clock, "c");

	    } else if (servicelock->flags & _CTX_LOCK_FLAG_READ) {

		result=get_read_access_ctx(servicelock, clock, "c");

	    }

	    if (result==0 || result==-1) return result;
	    servicelock->flags &= ~ _CTX_LOCK_FLAG_LOCK;

	} else {

	    /* unlock */

	    release_access_ctx(servicelock, clock, "c");

	}

    }

    return result;

}

static int _lock_workspace_contexes(struct service_context_lock_s *servicelock, struct system_timespec_s *expire)
{
    int result=0;
    struct service_context_s *root=servicelock->root;

    if (root && root->type==SERVICE_CTX_TYPE_WORKSPACE) {

	signal_lock(root->service.workspace.signal);

	while ((result=check_lockflags_conflict(servicelock))==0) {
	    int pthreadresult=0;

	    if (expire) {

		pthreadresult=signal_condtimedwait(root->service.workspace.signal, expire);

	    } else {

		pthreadresult=signal_condwait(root->service.workspace.signal);

	    }

	    if (check_lockflags_conflict(servicelock)==1) {

		result=1;
		break;

	    } else if (pthreadresult==ETIMEDOUT) {

		signal_unlock(root->service.workspace.signal);
		return -1;

	    } else if (pthreadresult>0) {

		logoutput_warning("execute_workspace_contexes_cb: error %i waiting condition (%s)", pthreadresult, strerror(pthreadresult));
		signal_unlock(root->service.workspace.signal);
		return -2;

	    }

	}

	signal_broadcast(root->service.workspace.signal);
	signal_unlock(root->service.workspace.signal);

    }

    return result;

}

static int _common_servicelock_ctx(struct service_context_lock_s *servicelock, unsigned int flag)
{
    int result=0;

    servicelock->flags |= flag;
    result=_lock_workspace_contexes(servicelock, NULL);
    servicelock->flags &= ~flag;

    return result;
}

static int _lock_service_context(struct service_context_lock_s *servicelock, const char *what, const char *how, unsigned int flag)
{

    logoutput_debug("_lock_service_context: what %s how %s", what, how);

    /* how says how to lock: read or write */

    if (how==NULL) {

	return -1;

    } else if (strcmp(how, "r")==0) {

	flag |= _CTX_LOCK_FLAG_READ;

    } else if (strcmp(how, "w")==0) {

	flag |= _CTX_LOCK_FLAG_WRITE;

    } else {

	return -1;

    }

    if (strcmp(what, "w")==0) {

	flag |= _CTX_LOCK_FLAG_ROOT;

    } else if (strcmp(what, "p")==0) {

	flag |= _CTX_LOCK_FLAG_PARENT;

    } else if (strcmp(what, "c")==0) {

	flag |= _CTX_LOCK_FLAG_SELF;

    } else {

	return -1;

    }

    return _common_servicelock_ctx(servicelock, flag);
}

int lock_service_context(struct service_context_lock_s *servicelock, const char *how, const char *what)
{
    return _lock_service_context(servicelock, what, how, _CTX_LOCK_FLAG_LOCK);
}

int unlock_service_context(struct service_context_lock_s *servicelock, const char *how, const char *what)
{
    return _lock_service_context(servicelock, what, how, 0);
}

void init_service_ctx_lock(struct service_context_lock_s *s, struct service_context_s *pctx, struct service_context_s *ctx)
{

    if (s) {

	s->flags=0;
	s->op=0;
	s->wlockflags=0;
	s->plockflags=0;
	s->clockflags=0;

	if (pctx) {

	    s->root=get_root_context(pctx);
	    s->pctx=pctx;

	} else if (ctx) {

	    s->root=get_root_context(ctx);
	    s->pctx=get_parent_context(ctx);

	} else {

	    s->root=NULL;
	    s->pctx=NULL;

	}

	s->ctx=ctx;

    }

}

void set_root_service_ctx_lock(struct service_context_s *root, struct service_context_lock_s *ctxlock)
{
    if (ctxlock && root) ctxlock->root=root;
}

void set_ctx_service_ctx_lock(struct service_context_s *ctx, struct service_context_lock_s *ctxlock)
{

    if (ctxlock && ctx) {

	ctxlock->ctx=ctx;
	if (ctxlock->pctx==NULL) ctxlock->pctx=get_parent_context(ctx);
	if (ctxlock->root==NULL) ctxlock->root=get_root_context(ctx);

    }

}

void set_workspace_ctx_lock(struct workspace_mount_s *w, struct service_context_lock_s *ctxlock)
{
    struct list_header_s *h=&w->contexes;
    struct list_element_s *list=get_list_head(h);
    struct service_context_s *root=NULL;

    if (list) {

	root=(struct service_context_s *)((char *) list - offsetof(struct service_context_s, wlist));
	set_root_service_ctx_lock(root, ctxlock);

    }

}

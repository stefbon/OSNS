/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_WORKSPACE_CONTEXT_LOCK_H
#define _LIB_WORKSPACE_CONTEXT_LOCK_H

#define SERVICE_CTX_LOCK_OP_NEXT		1
#define SERVICE_CTX_LOCK_OP_PREV		2
#define SERVICE_CTX_LOCK_OP_ADD			3
#define SERVICE_CTX_LOCK_OP_DEL			4
#define SERVICE_CTX_LOCK_OP_EDIT		5
#define SERVICE_CTX_LOCK_OP_READ		6
#define SERVICE_CTX_LOCK_OP_RM			7

#define SERVICE_CTX_LOCK_OP_NEXT_W		8
#define SERVICE_CTX_LOCK_OP_PREV_W		9
#define SERVICE_CTX_LOCK_OP_ADD_W		10
#define SERVICE_CTX_LOCK_OP_DEL_W		11

#define SERVICE_CTX_LOCK_FLAG_LOCK		1

struct service_context_lock_s {
    unsigned char				flags;
    unsigned char				op;
    unsigned int				wlockflags;
    unsigned int				plockflags;
    unsigned int				clockflags;
    struct service_context_s			*root;
    struct service_context_s			*pctx;
    struct service_context_s			*ctx;
};

#define SERVICE_CTX_LOCK_INIT			{0, 0, 0, 0,0, NULL, NULL, NULL}

/* prototypes */

int lock_service_context(struct service_context_lock_s *servicelock, const char *how, const char *what);
int unlock_service_context(struct service_context_lock_s *servicelock, const char *how, const char *what);
void init_service_ctx_lock(struct service_context_lock_s *s, struct service_context_s *pctx, struct service_context_s *ctx);
void set_root_service_ctx_lock(struct service_context_s *root, struct service_context_lock_s *ctxlock);
void set_ctx_service_ctx_lock(struct service_context_s *ctx, struct service_context_lock_s *ctxlock);

#endif

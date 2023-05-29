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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "fuse/browse-fs.h"

#include "osns_client.h"
#include "network.h"
#include "utils.h"
#include "resources.h"
#include "contexes.h"

struct network_service_target_s {
    uint64_t dbid;
    struct ip_address_s ip;
    struct network_service_s netsrv;
};

struct network_service_lookup_hlpr_s {
    unsigned int flags;
    unsigned int service;
    struct network_service_target_s *target;
};

static void read_network_address_hlpr(struct network_resource_s *r, void *ptr)
{
    struct network_service_lookup_hlpr_s *hlpr=(struct network_service_lookup_hlpr_s *) ptr;

    if (r->type==NETWORK_RESOURCE_TYPE_ADDRESS) {

        if (r->flags & NETWORK_RESOURCE_FLAG_IPv4) {

            memcpy(hlpr->target[0].ip.addr.v4, r->data.ipv4, INET_ADDRSTRLEN);
            hlpr->target[0].ip.family=IP_ADDRESS_FAMILY_IPv4;
            hlpr->flags |= NETWORK_RESOURCE_FLAG_IPv4;
            hlpr->target[0].dbid=r->id;

        } else if (r->flags & NETWORK_RESOURCE_FLAG_IPv6) {

            memcpy(hlpr->target[1].ip.addr.v6, r->data.ipv6, INET6_ADDRSTRLEN);
            hlpr->target[1].ip.family=IP_ADDRESS_FAMILY_IPv6;
            hlpr->flags |= NETWORK_RESOURCE_FLAG_IPv6;
            hlpr->target[1].dbid=r->id;

        }

    } else if (r->type==NETWORK_RESOURCE_TYPE_SERVICE) {

        logoutput_debug("read_network_address_hlpr: found network service record service=%u pid=%u", r->data.service.service, r->pid);

        if (r->data.service.service==hlpr->service) {

            if (r->pid==hlpr->target[0].dbid) {

                memcpy(&hlpr->target[0].netsrv, &r->data.service, sizeof(struct network_service_s));

            } else if (r->pid==hlpr->target[1].dbid) {

                memcpy(&hlpr->target[1].netsrv, &r->data.service, sizeof(struct network_service_s));

            }

        }

    }

}

static int get_host_service_address(uint64_t dbid, unsigned int service, struct network_service_lookup_hlpr_s *hlpr)
{
    struct db_query_result_s result=DB_QUERY_RESULT_INIT;
    int tmp=-1;

    logoutput_debug("get_host_service_address: unable to browse addresses: dbid %lu service %u", dbid, service);

    if (browse_client_network_data(dbid, NETWORK_RESOURCE_TYPE_ADDRESS, read_network_address_hlpr, &result, (void *) hlpr)==0) {
        uint64_t pid=0;

        /* test the different addresses found
            20221215: two different types of addresses: ipv4 and ipv6
            store ipv4 address (if available) in first array element
            store ipv6 address (if available) in second array element
        */

        for (unsigned int i=0; i<2; i++) {

            if (hlpr->target[i].dbid>0) {

                /* get the network services from the network data db which match the service */

                if (browse_client_network_data(hlpr->target[i].dbid, NETWORK_RESOURCE_TYPE_SERVICE, read_network_address_hlpr, &result, (void *) hlpr)==0) {

                    logoutput_debug("get_host_service_address: browse services success (i=%u)", i);
                    tmp=0;

                } else {

                    logoutput_debug("get_host_service_address: unable to browse services (i=%u)", i);

                }

            }

        }

    } else {

        logoutput_debug("get_host_service_address: unable to browse addresses");

    }

    return tmp;

}

static struct service_context_s *connect_context_service_address(struct workspace_mount_s *w, uint64_t dbid, unsigned int service, struct ip_address_s *ip, struct network_port_s *port)
{
    struct service_context_s *sharedctx=NULL;
    struct context_interface_s *i=NULL;
    struct interface_status_s istatus;
    char *name="unknown";

    if (ip->family==IP_ADDRESS_FAMILY_IPv4) {

        logoutput_debug("connect_context_service_address: connect to ipv4 %s:%u service %s", ip->addr.v4, port->nr, get_network_service_name(service));
        name=ip->addr.v4;

    } else if (ip->family==IP_ADDRESS_FAMILY_IPv6) {

        logoutput_debug("connect_context_service_address: connect to ipv6 %s:%u service %s", ip->addr.v6, port->nr, get_network_service_name(service));
        name=ip->addr.v6;

    } else {

        logoutput_debug("connect_context_service_address: connect to unknown:%u service %s", port->nr, get_network_service_name(service));

    }

    if (service==NETWORK_SERVICE_TYPE_SSH) {

        sharedctx=create_network_shared_context(w, dbid, service, NETWORK_SERVICE_TYPE_SSH, _INTERFACE_TYPE_SSH_SESSION, NULL);

        if (sharedctx==NULL) {

            logoutput_debug("connect_context_service_address: unable to create shared context for service %u:%s", service, get_network_service_name(service));
            return NULL;

        }

    }

    if (sharedctx==NULL) {

        logoutput_debug("connect_context_service_address: service %u:%s not supported", service, get_network_service_name(service));
        return NULL;

    }

    /* 20221127: test for errors, only apply when dealing with an existing ctx, which did not connect
        due to errors ... this call will get these errors
        todo when a non fatal error retry after some time ... and then what will the timeout be?? */

    i=&sharedctx->interface;
    init_interface_status(&istatus);

    if ((* i->get_interface_status)(i, &istatus)>0) {

        if (istatus.flags & (INTERFACE_STATUS_FLAG_ERROR | INTERFACE_STATUS_FLAG_DISCONNECTED)) {

            /* 20221214: return the sharedctx as a disfunctional ctx ...
                TODO try to find out it has sense to reconnect */
            return sharedctx;

        }

    }

    if ((i->flags & _INTERFACE_FLAG_CONNECT)==0) {
        union interface_target_u target = {.ip=ip};
        union interface_parameters_u param = {.port=port};

        /* connect */

        if ((* i->connect)(i, &target, &param)>=0) {

            logoutput_debug("connect_context_service_address: addr %s service %u:%s connected", name, service, get_network_service_name(service));

            if ((* i->start)(i)>=0) {

                logoutput_debug("connect_context_service_address: addr %s service %u:%s started", name, service, get_network_service_name(service));

            } else {

                logoutput_debug("connect_context_service_address: unable to start service %u:%s", service, get_network_service_name(service));

            }

        }

    }

    out:
    return sharedctx;

}

unsigned int connect_context_service(struct service_context_s *ctx, unsigned int service, void (* cb_success)(struct service_context_s *ctx, uint64_t dbid, void *ptr), void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr), void *ptr)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    unsigned int errcode=0;
    struct network_service_target_s target[2]; /* two different types of address families: ipv4 and ipv6 */
    struct network_service_lookup_hlpr_s hlpr;

    if ((ctx->type != SERVICE_CTX_TYPE_BROWSE) || ((ctx->type == SERVICE_CTX_TYPE_BROWSE) && (ctx->service.browse.type != SERVICE_BROWSE_TYPE_NETHOST))) {

        errcode=EINVAL;
        goto errorout;

    }

    if (w==NULL) {

        errcode=ENXIO;
        goto errorout;

    }

    if (ctx->service.browse.unique==0) {

        errcode=ENODATA; /* error No data available ... is there a better one ? (the network resource is not available) */
        goto errorout;

    }

    /* look for the desired service/address for this host in the db
        is it possible more than one address is available?
        20221215: yes, two*/

    memset(target, 0, 2 * sizeof(struct network_service_target_s));
    hlpr.flags=0;
    hlpr.service=service;
    hlpr.target=target;

    if (get_host_service_address(ctx->service.browse.unique, service, &hlpr)==0) {

        /* try ipv4 first if available */

        if ((hlpr.flags & NETWORK_RESOURCE_FLAG_IPv4) && (hlpr.target[0].netsrv.service==service)) {
            struct service_context_s *sharedctx=connect_context_service_address(w, hlpr.target[0].dbid, service, &hlpr.target[0].ip, &hlpr.target[0].netsrv.port);

            if (sharedctx) {

                (* cb_success)(sharedctx, hlpr.target[0].dbid, ptr);
                return 0;

            }

        }

        /* try ipv6 if available */

        if ((hlpr.flags & NETWORK_RESOURCE_FLAG_IPv6) && (hlpr.target[1].netsrv.service==service)) {
            struct service_context_s *sharedctx=connect_context_service_address(w, hlpr.target[1].dbid, service, &hlpr.target[1].ip, &hlpr.target[1].netsrv.port);

            if (sharedctx) {

                (* cb_success)(sharedctx, hlpr.target[1].dbid, ptr);
                return 0;

            }

        }

    } else {

        errcode=ENODATA;

    }

    errorout:
    if (errcode==0) errcode=EIO;
    (* cb_error)(ctx, errcode, ptr);
    return errcode;

}

struct connect_browse_service_s {
    struct service_context_s                                    *sctx;
    struct service_context_s                                    *bctx;
    uint64_t                                                    dbid;
    unsigned int                                                errcode;
};

/* cb for success */
static void success_browse_service_hlpr(struct service_context_s *sctx, uint64_t dbid, void *ptr)
{
    struct connect_browse_service_s *hlpr=(struct connect_browse_service_s *) ptr;
    struct service_context_s *bctx=hlpr->bctx;
    struct service_context_s *root=get_root_context(bctx);
    struct shared_signal_s *s=root->service.workspace.signal;

    hlpr->sctx=sctx;
    hlpr->dbid=dbid;

    /* link the browse ctx and the shared ctx */

    logoutput_debug("success_browse_service_hlpr: link primary and secundary");
    (* sctx->interface.set_secondary)(&sctx->interface, &bctx->interface);
    (* bctx->interface.set_primary)(&bctx->interface, &sctx->interface);

    signal_broadcast_locked(s);

}

/* cb for error */
static void error_browse_service_hlpr(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct connect_browse_service_s *hlpr=(struct connect_browse_service_s *) ptr;
    struct service_context_s *bctx=hlpr->bctx;
    struct service_context_s *root=get_root_context(bctx);
    struct shared_signal_s *s=root->service.workspace.signal;

    logoutput_debug("error_browse_service_hlpr: errcode %u", errcode);
    hlpr->errcode=errcode;
    bctx->interface.flags |= _INTERFACE_FLAG_ERROR;
    signal_broadcast_locked(s);
}

/* after a service context which represents a network host in the tree in contexes is found a connection to the server has to be made
    this procedure takes care of that
    it does that by looking for addresses which offer the service which matches the service this tree of contexes is created for
    for example a host in the network (detected by dnssd for example) has ipnumber 192.168.0.20, and it offers the service SSH on port nr 22 type tcp

    the addresses and services are searched for in the network db, and if found, here a connection to the SSH server is made
    at this moment 20221211 first ipv4 (if this address is present in the network db and the SSH service is found in this address of this host,
    this is tried first. If that succeeds, this function is ready.
    If it fails to make a connection, a connection is made on the ipv6 address, if found in the db, and the SSH service is available on this address
    according the network db.

*/

static void thread_connect_browse_service(void *ptr)
{
    struct service_context_s *ctx=(struct service_context_s *) ptr;
    struct connect_browse_service_s hlpr;

    hlpr.bctx=ctx;
    hlpr.sctx=NULL; /* to be found and to connect */
    hlpr.dbid=0;
    hlpr.errcode=0;

    if (connect_context_service(ctx, ctx->service.browse.service, success_browse_service_hlpr, error_browse_service_hlpr, (void *) &hlpr)==0) {

        logoutput_debug("thread_connect_browse_service: ctx connected");

    }

}

void start_thread_connect_browse_service(struct service_context_s *ctx)
{

    if (ctx && (ctx->type==SERVICE_CTX_TYPE_BROWSE) && (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST)) {

        if (ctx->interface.link.primary) {

            logoutput_debug("start_thread_connect_browse_service: primary interface already defined ....");
            return;

        }

        logoutput_debug("start_thread_connect_browse_service: starting thread to connect %u:%s", ctx->type, ctx->name);
        work_workerthread(NULL, 0, thread_connect_browse_service, (void *) ctx);

    }

}

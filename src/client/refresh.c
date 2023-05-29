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
#include "interface/sftp.h"

#include "osns_client.h"
#include "network.h"
#include "utils.h"
#include "connect.h"
#include "resources.h"
#include "ssh.h"
#include "sftp.h"

static struct service_context_s *find_related_shared_service_context(struct service_context_s *ctx)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct list_header_s *h=&w->shared_contexes;
    struct service_context_s *sctx=NULL;

    logoutput_debug("find_related_shared_service_context: ctx browse type %u:%s service %u", ctx->service.browse.type, ctx->name, ctx->service.browse.service);

    /* make sure there is no more than one thread busy detecting resources for this ctx */

    read_lock_list_header(h);
    sctx=get_next_shared_service_context(w, NULL);

    while (sctx) {

        if (sctx->service.shared.service==ctx->service.browse.service) {

            /* look for shared context with the same service and
                db id's are related */

            if ((ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST) && (ctx->service.browse.unique>0) && 
                (get_parent_id_network_resource(sctx->service.shared.unique)==ctx->service.browse.unique)) break;


        }

        sctx=get_next_shared_service_context(w, sctx);

    }

    read_unlock_list_header(h);
    return sctx;

}

struct populate_network_host_hlpr_s {
    struct service_context_s *ctx;
    int result;
};

static void add_service_ssh_host(struct service_context_s *SSH_session_ctx, char *name, unsigned int len, void *ptr)
{
    unsigned int pos=0;

    if (compare_starting_substring(name, len, "ssh-channel://", &pos)==0) {
        unsigned char tmpused=0;

        if (compare_starting_substring(&name[pos], (len - pos), "session/subsystem/sftp", &pos)==0) {

            if ((pos==len) || ((pos<len) && (name[pos]=='/'))) {
                struct populate_network_host_hlpr_s *hlpr=(struct populate_network_host_hlpr_s *) ptr;
                unsigned char action=0;
                struct service_context_s *FS_ctx=create_sftp_filesystem_browse_context(SSH_session_ctx, hlpr->ctx, name, len, &action);

                tmpused=1;
                if (FS_ctx) {

                    logoutput_debug("add_service_ssh_host: found name %.*s using ssh channel to start sftp subsystem", len, name);
                    hlpr->result=0;

                }

            }

        }

        if (tmpused==0) logoutput_debug("add_service_ssh_host: (not using) name %s", name);

    } else {

        logoutput_debug("add_service_ssh_host(: name %s not reckognized", name);

    }

}

static void process_properties_remote_service(struct service_context_s *SSH_session_ctx, char *data, unsigned int size, void *ptr)
{
    struct populate_network_host_hlpr_s *hlpr=(struct populate_network_host_hlpr_s *) ptr;
    struct service_context_s *SFTP_browse_client_ctx=hlpr->ctx;
    struct service_context_s *SFTP_shared_client_ctx=NULL;
    unsigned char action=0;
    char *sep=NULL;
    char *home=NULL;
    int result=-1;

    if (SFTP_browse_client_ctx->type!=SERVICE_CTX_TYPE_FILESYSTEM) return;

    /* process a reply from ssh host when asking for a service */

    if ((data==NULL) || (size==0)) {

        logoutput_debug("process_properties_remote_service: no data received");
        return;

    }

    logoutput_debug("process_properties_remote_service: data size %u ctx type %u", size, SFTP_browse_client_ctx->type);

    /* data has simple format like:

         %PREFIX%|%OTHERDATA1%|%OTHERDATA2%|

            20230223: think about another format .... a parameter - value approach ... json??
    */

    sep=memchr(data, '|', size);

    if (sep==NULL) {

        logoutput_debug("process_properties_remote_service: no seperator found, cannot continue");
        return;

    }

    *sep='\0';
    logoutput_debug("process_properties_remote_service: prefix found %s", data);

    home=get_ssh_session_remote_home(&SSH_session_ctx->interface);
    result=set_prefix_sftp_browse_client(&SFTP_browse_client_ctx->interface, data, home);

    if (result==0 || result==-1) {

        logoutput_debug("process_properties_remote_service: unable to set prefix (return value set prefix %i)", result);
        return;

    }

    SFTP_shared_client_ctx=create_sftp_filesystem_shared_context(SSH_session_ctx, &action);

    if (SFTP_shared_client_ctx) {
        struct context_interface_s *i=&SFTP_shared_client_ctx->interface;
        union interface_target_u target;
        union interface_parameters_u param;

        (* SFTP_browse_client_ctx->interface.set_primary)(&SFTP_browse_client_ctx->interface, i);
        (* i->set_secondary)(i, &SFTP_browse_client_ctx->interface);

        if (action==CHECK_INSTALL_CTX_ACTION_ADD) {

            logoutput_debug("process_properties_remote_service: created sftp shared context for %s", data);

            /* connect */

            if ((* i->connect)(i, &target, &param)>=0) {

                logoutput_debug("process_properties_remote_service: connected");

                if ((* i->start)(i)>=0) {

                    logoutput_debug("process_properties_remote_service: started");
                    hlpr->result=0;

                } else {

                    logoutput_debug("process_properties_remote_service: unable to start");

                }

            }

        } else {

            hlpr->result=0;
            logoutput_debug("process_properties_remote_service: found sftp shared context for %s", data);

        }

    } else {

        logoutput_debug("process_ssh_host_service: unable to create sftp shared context for %s", data);

    }

}

/* perform action after a lookup is done (on a FUSE browse map) for SERVICE CONTEXT representing network host */

int refresh_network_host_lookup(struct service_context_s *ctx)
{
    struct service_context_s *root=NULL;
    struct shared_signal_s *signal=NULL;
    struct system_timespec_s refresh=SYSTEM_TIME_INIT;
    struct system_timespec_s now=SYSTEM_TIME_INIT;

    if ((ctx==NULL) || (ctx->type != SERVICE_CTX_TYPE_BROWSE) || (ctx->service.browse.type != SERVICE_BROWSE_TYPE_NETHOST)) {

        logoutput_warning("refresh_network_host_lookup: wrong type, expecting a browse network host");
        return 0;

    } else if ((* ctx->interface.get_primary)(&ctx->interface)) {

        logoutput_warning("refresh_network_host_lookup: already connected");
        return 0;

    }

    root=get_root_context(ctx);
    signal=root->service.workspace.signal;
    logoutput_debug("refresh_network_host_lookup: ctx service %u unique %u", ctx->service.browse.service, ctx->service.browse.unique);
    if (signal_set_flag(signal, &ctx->service.browse.status, SERVICE_BROWSE_FLAG_REFRESH_LOOKUP)==0) return -1;

    /* minimal 6 seconds between to refreshes */

    copy_system_time(&refresh, &ctx->service.browse.refresh_lookup);
    system_time_add(&refresh, SYSTEM_TIME_ADD_ZERO, 6);
    ctx->service.browse.threadid=pthread_self();
    get_current_time_system_time(&now);

    if (system_time_test_earlier(&refresh, &now)==1) {

        start_thread_connect_browse_service(ctx);
        get_current_time_system_time(&ctx->service.browse.refresh_lookup);

    }

    unlockout:

    ctx->service.browse.threadid=0;
    signal_unset_flag(signal, &ctx->service.browse.status, SERVICE_BROWSE_FLAG_REFRESH_LOOKUP);
    return 0;

}

/* perform action before an opendir is done on an entry linked to a SERVICE CONTEXT representing a network host */

int refresh_network_host_opendir(struct service_context_s *ctx)
{
    struct system_timespec_s refresh=SYSTEM_TIME_INIT;
    struct system_timespec_s now=SYSTEM_TIME_INIT;
    struct service_context_s *root=NULL;
    struct shared_signal_s *signal=NULL;
    int result=-1;

    if ((ctx==NULL) || (ctx->type != SERVICE_CTX_TYPE_BROWSE) || (ctx->service.browse.type != SERVICE_BROWSE_TYPE_NETHOST)) {

        logoutput_warning("refresh_network_host_opendir: wrong type, expecting a browse network host");
        return 0;

    }

    root=get_root_context(ctx);
    signal=root->service.workspace.signal;

    if (signal_set_flag(signal, &ctx->service.browse.status, SERVICE_BROWSE_FLAG_REFRESH_OPENDIR)==0) return -1;

    /* minimal 6 seconds between to refreshes */

    copy_system_time(&refresh, &ctx->service.browse.refresh_opendir);
    ctx->service.browse.threadid=pthread_self();
    system_time_add(&refresh, SYSTEM_TIME_ADD_ZERO, 6);
    get_current_time_system_time(&now);

    if (system_time_test_earlier(&refresh, &now)==1) {
        struct context_interface_s *i=&ctx->interface;

        logoutput_debug("refresh_network_host_opendir: ctx service %u unique %u", ctx->service.browse.service, ctx->service.browse.unique);

        signal_lock(signal);

        while (((* i->get_primary)(i)==NULL) && (ctx->interface.flags & _INTERFACE_FLAG_ERROR)==0) {

            int tmp=signal_condtimedwait(signal, &now);
            if (tmp==ETIMEDOUT) break;

        }

        signal_unlock(signal);

        if ((* i->get_primary)(i)) {
            struct populate_network_host_hlpr_s hlpr={.ctx=ctx, .result=-1};
            struct service_context_s *SSH_session_ctx=(struct service_context_s *)((char *)(* i->get_primary)(i) - offsetof(struct service_context_s, interface));

            result=(int) get_remote_services_ssh_host(SSH_session_ctx, add_service_ssh_host, (void *) &hlpr);
            get_current_time_system_time(&ctx->service.browse.refresh_opendir);

        }

    }

    unlockout:

    ctx->service.browse.threadid=0;
    signal_unset_flag(signal, &ctx->service.browse.status, SERVICE_BROWSE_FLAG_REFRESH_OPENDIR);
    return result;

}

int refresh_network_service_lookup(struct service_context_s *BROWSE_nethost_ctx, struct service_context_s *FS_ctx)
{
    struct system_timespec_s refresh=SYSTEM_TIME_INIT;
    struct system_timespec_s now=SYSTEM_TIME_INIT;
    struct shared_signal_s *signal=NULL;
    struct service_context_s *SSH_session_ctx=NULL;
    struct context_interface_s *i=NULL;
    int result=-1;

    if ((BROWSE_nethost_ctx==NULL) || (BROWSE_nethost_ctx->type != SERVICE_CTX_TYPE_BROWSE) || (BROWSE_nethost_ctx->service.browse.type != SERVICE_BROWSE_TYPE_NETHOST)) {

        logoutput_warning("refresh_network_service_lookup: wrong type, expecting a browse network host");
        return 0;

    } else if ((FS_ctx==NULL) || (FS_ctx->type != SERVICE_CTX_TYPE_FILESYSTEM)) {

        logoutput_warning("refresh_network_service_lookup: wrong argument, expecting a filesystem service");
        return 0;

    }

    i=&FS_ctx->interface;
    if ((* i->get_primary)(i)) return 0;

    signal=FS_ctx->service.filesystem.signal;

    if (signal_set_flag(signal, &FS_ctx->flags, SERVICE_CTX_FLAG_REFRESH)==0) return -1;

    /* minimal 6 seconds between to refreshes */

    copy_system_time(&refresh, &FS_ctx->service.filesystem.refresh);
    system_time_add(&refresh, SYSTEM_TIME_ADD_ZERO, 6);
    get_current_time_system_time(&now);

    if (system_time_test_earlier(&refresh, &now)==-1) {

        logoutput_debug("refresh_network_service_lookup: cannot refresh, minimal time between two refresh not expired");
        goto unlockout;

    }

    system_time_add(&now, SYSTEM_TIME_ADD_ZERO, 6);

    i=&BROWSE_nethost_ctx->interface;
    if ((* i->get_primary)(i)) SSH_session_ctx=(struct service_context_s *)((char *) (* i->get_primary)(i) - offsetof(struct service_context_s, interface));

    if (SSH_session_ctx) {
        struct populate_network_host_hlpr_s hlpr={.ctx=FS_ctx, .result=-1};
        unsigned int count=get_remote_service_ssh_host(SSH_session_ctx, FS_ctx->service.filesystem.name, process_properties_remote_service, (void *) &hlpr);

        logoutput_debug("refresh_network_service_lookup: shared ctx found service %u unique %lu", SSH_session_ctx->service.shared.service, SSH_session_ctx->service.shared.unique);
        result=(int) hlpr.result;

    }

    get_current_time_system_time(&FS_ctx->service.filesystem.refresh);

    unlockout:
    signal_unset_flag(signal, &FS_ctx->flags, SERVICE_CTX_FLAG_REFRESH);
    return result;

}

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <dirent.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "misc.h"

#include "threads.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "discover.h"

#include "fuse/sftp.h"
#include "fuse/ssh.h"

extern struct fs_options_s fs_options;

struct entry_s *create_network_map_entry(struct service_context_s *context, struct directory_s *directory, struct name_s *xname, unsigned int *error)
{
    struct create_entry_s ce;
    struct stat st;

    /* stat values for a network map */

    st.st_mode=S_IFDIR | S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    st.st_uid=0;
    st.st_gid=0;
    st.st_ino=0;
    st.st_dev=0;
    st.st_nlink=2;
    st.st_rdev=0;
    st.st_size=_INODE_DIRECTORY_SIZE;
    st.st_blksize=1024;
    st.st_blocks=(unsigned int) (st.st_size / st.st_blksize) + ((st.st_size % st.st_blksize)==0) ? 1 : 0;

    get_current_time(&st.st_mtim);
    memcpy(&st.st_ctim, &st.st_mtim, sizeof(struct timespec));
    memcpy(&st.st_atim, &st.st_mtim, sizeof(struct timespec));

    init_create_entry(&ce, xname, NULL, directory, NULL, context, &st, NULL);
    return create_entry_extended_batch(&ce);

}


/* function called when a network FUSE context is mounted */

static void install_net_services_context(struct host_address_s *host, struct service_address_s *service, unsigned int code, struct timespec *found, unsigned long hostid, unsigned int serviceid, void *ptr)
{
    struct service_context_s *context=(struct service_context_s *) ptr;
    struct workspace_mount_s *workspace=context->workspace;
    struct inode_s *inode=NULL;
    struct directory_s *root_directory=NULL;
    struct simple_lock_s wlock;
    unsigned int error=0;
    char *target=NULL;
    unsigned int port=0;

    logoutput("install_net_services_context");

    if (workspace->syncdate.tv_sec < found->tv_sec || (workspace->syncdate.tv_sec == found->tv_sec && workspace->syncdate.tv_nsec < found->tv_nsec)) {

	workspace->syncdate.tv_sec = found->tv_sec;
	workspace->syncdate.tv_nsec = found->tv_nsec;

    }

    inode=&workspace->inodes.rootinode;
    root_directory=get_directory(inode);

    if (wlock_directory(root_directory, &wlock)==-1) {

	logoutput("install_net_services_context: unable to lock root directory");
	return;

    }

    unlock_directory(root_directory, &wlock);

    translate_context_host_address(host, &target, NULL);
    translate_context_network_port(service, &port);

    logoutput("install_net_services_context: connecting to %s:%i", target, port);

    if (code==WORKSPACE_SERVICE_SMB) {

	logoutput("install_net_services_context: SMB noy supported yet");

    } else if (code==WORKSPACE_SERVICE_SSH) {

	int result=install_ssh_server_context(workspace, inode->alias, host, service, &error);

	if (result != 0) logoutput("install_net_services_context: unable to connect to %s:%i error %i:%s", target, port, error, strerror(error));

    }

}

/* function called when a network service on a host is detected
    walk every FUSE context for network services and test it should be used here
*/

static void install_net_services_all(struct host_address_s *host, struct service_address_s *service, unsigned int code, struct timespec *found, unsigned long hostid, unsigned long serviceid)
{
    struct fuse_user_s *user=NULL;
    unsigned int hashvalue=0;
    void *index=NULL;
    struct list_element_s *wlist=NULL;
    struct simple_lock_s rlock;

    logoutput("install_net_services_all: host:service id %i:%i code %i", hostid, serviceid, code);

    init_rlock_users_hash(&rlock);

    rlock:
    lock_users_hash(&rlock);

    nextuser:

    user=get_next_fuse_user(&index, &hashvalue);
    if (user==NULL) {

	logoutput("install_net_services_all: ready");
	unlock_users_hash(&rlock);
	return;

    }

    logoutput("install_net_services_all: found user %i: %s", user->pwd.pw_uid, user->pwd.pw_name);

    pthread_mutex_lock(&user->mutex);
    user->flags |= FUSE_USER_FLAG_INSTALL_SERVICES;
    pthread_mutex_unlock(&user->mutex);
    unlock_users_hash(&rlock);

    /* walk every workspace (=mount) for this user */

    wlist=get_list_head(&user->workspaces, 0);

    while (wlist) {
	struct workspace_mount_s *workspace=NULL;
	struct list_element_s *clist=NULL;
	struct service_context_s *context=NULL;

	workspace=get_container_workspace(wlist);

	/* test service is already in use on this workspace
	    if not install it */

	clist=get_list_head(&workspace->contexes, 0);

	while (clist) {

	    context=get_container_context(clist);
	    if ((context->type==SERVICE_CTX_TYPE_FILESYSTEM ||
		context->type==SERVICE_CTX_TYPE_CONNECTION ||
		context->type==SERVICE_CTX_TYPE_SOCKET) && context->serviceid==serviceid) break;
	    clist=get_next_element(clist);
	    context=NULL;

	}

	/* only install when not found
	    TODO: when it's found earlier (does exist) but not connected does this here again */

	if (! context) {

	    context=get_workspace_context(workspace);
	    install_net_services_context(host, service, code, found, hostid, serviceid, (void *) context);

	}

	wlist=get_next_element(wlist);

    }

    pthread_mutex_lock(&user->mutex);
    user->flags -= FUSE_USER_FLAG_INSTALL_SERVICES;
    pthread_mutex_unlock(&user->mutex);

    goto rlock;

}

void install_net_services_cb(struct host_address_s *host, struct service_address_s *service, unsigned int code, struct timespec *found, unsigned long hostid, unsigned long serviceid, void *ptr)
{

    logoutput("install_net_services_cb");

    switch (code) {

	case WORKSPACE_SERVICE_SSH:

	    if ((fs_options.network.services & _OPTIONS_NETWORK_ENABLE_SSH)==0) return;
	    break;

	case WORKSPACE_SERVICE_SMB:

	    if ((fs_options.network.services & _OPTIONS_NETWORK_ENABLE_SMB)==0) return;
	    break;

	case WORKSPACE_SERVICE_WEBDAV:

	    if ((fs_options.network.services & _OPTIONS_NETWORK_ENABLE_WEBDAV)==0) return;
	    break;

	default:

	    logoutput("install_net_services_cb: service %i not supported", code);
	    return;

    }

    if (ptr) {

	install_net_services_context(host, service, code, found, hostid, serviceid, ptr);

    } else {

	install_net_services_all(host, service, code, found, hostid, serviceid);

    }

}

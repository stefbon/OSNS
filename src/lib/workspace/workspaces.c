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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fsuid.h>
#include <sys/mount.h>

#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "misc.h"
#include "misc.h"
#include "eventloop.h"
#include "threads.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#undef LOGGING
#include "log.h"

extern const char *dotdotname;
extern const char *dotname;
static struct list_header_s workspaces=INIT_LIST_HEADER;
static pthread_mutex_t workspaces_mutex=PTHREAD_MUTEX_INITIALIZER;

int lock_workspaces()
{
    return pthread_mutex_lock(&workspaces_mutex);
}

int unlock_workspaces()
{
    return pthread_mutex_unlock(&workspaces_mutex);
}

struct workspace_mount_s *get_next_workspace_mount(struct workspace_mount_s *w)
{
    struct list_element_s *list=NULL;

    if (w) {

	list=&w->list_g;
	list=get_next_element(list);

    } else {

	list=get_list_head(&workspaces, 0);

    }

    return ((list) ? (struct workspace_mount_s *) ((char *) list - offsetof(struct workspace_mount_s, list_g)) : NULL);
}

struct inode_2delete_s {
    ino_t				ino;
    unsigned int			flags;
    uint64_t				forget;
    struct list_element_s		list;
};

void adjust_pathmax(struct workspace_mount_s *w, unsigned int len)
{
    pthread_mutex_lock(&w->mutex);
    if (len>w->pathmax) w->pathmax=len;
    pthread_mutex_unlock(&w->mutex);
}

unsigned int get_pathmax(struct workspace_mount_s *w)
{
    return w->pathmax;
}

void clear_workspace_mount(struct workspace_mount_s *workspace)
{
    unsigned int error=0;
    struct directory_s *directory=remove_directory(&workspace->inodes.rootinode, &error);

    logoutput("clear_workspace_mount: mountpoint %s", workspace->mountpoint.path);

    if (directory) {
	struct service_context_s *context=get_root_context_workspace(workspace);

	clear_directory_recursive(&context->interface, directory);
	free_directory(directory);

    }

}

static void init_workspace_inodes(struct workspace_mount_s *workspace)
{
    struct workspace_inodes_s *inodes=&workspace->inodes;
    struct inode_s *inode=&inodes->rootinode;
    struct entry_s *entry=&inodes->rootentry;
    struct stat *st=&inode->st;

    init_inode(inode);
    inode->nlookup=1;
    inode->fs=NULL;
    inode->alias=entry;

    /* root entry */

    init_entry(entry);
    entry->inode=inode;
    entry->flags=_ENTRY_FLAG_ROOT;

    /* root inode stat */

    get_current_time(&st->st_mtim);
    st->st_ctim.tv_sec=st->st_mtim.tv_sec;
    st->st_ctim.tv_nsec=st->st_mtim.tv_nsec;
    st->st_ino=FUSE_ROOT_ID;
    st->st_mode=S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    st->st_nlink=2;
    st->st_uid=0;
    st->st_gid=0;
    st->st_size=_INODE_DIRECTORY_SIZE;
    st->st_blksize=1024;
    st->st_blocks=(unsigned int) (st->st_size / st->st_blksize) + ((st->st_size % st->st_blksize)==0) ? 1 : 0;

    /* inodes hashtable */

    for (unsigned int i=0; i<WORKSPACE_INODE_HASHTABLE_SIZE; i++) init_list_header(&inodes->hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    inodes->nrinodes=1;
    inodes->inoctr=FUSE_ROOT_ID;
    init_list_header(&inodes->forget, SIMPLE_LIST_TYPE_EMPTY, NULL);

    /* a mutex and cond combination for protection and signalling */

    pthread_mutex_init(&inodes->mutex, NULL);
    pthread_cond_init(&inodes->cond, NULL);
    inodes->thread=0;

}

static void free_workspace_inodes(struct workspace_mount_s *workspace)
{
    struct workspace_inodes_s *inodes=&workspace->inodes;

    for (unsigned int i=0; i<WORKSPACE_INODE_HASHTABLE_SIZE; i++) {
	struct list_element_s *list=get_list_head(&inodes->hashtable[i], SIMPLE_LIST_FLAG_REMOVE);

	while (list) {
	    struct list_element_s *next=get_next_element(list);
	    struct inode_s *inode=(struct inode_s *)((char *) list - offsetof(struct inode_s, list));

	    free(inode);
	    list=next;

	}

    }

    pthread_mutex_destroy(&inodes->mutex);
    pthread_cond_destroy(&inodes->cond);

}

void free_workspace_mount(struct workspace_mount_s *workspace)
{
    remove_list_element(&workspace->list_g);
    free_workspace_inodes(workspace);
    free_path_pathinfo(&workspace->mountpoint);
    pthread_mutex_destroy(&workspace->mutex);
    free(workspace);
}

static void mountevent_dummy(struct workspace_mount_s *workspace, unsigned char event)
{

    if (event==WORKSPACE_MOUNT_EVENT_MOUNT || event==WORKSPACE_MOUNT_EVENT_UMOUNT) {

	logoutput_info("mountevent_dummy: workspace %.*s %smounted", workspace->mountpoint.len, workspace->mountpoint.path, (event==WORKSPACE_MOUNT_EVENT_UMOUNT) ? "u" : "");

    } else {

	logoutput_warning("mountevent_dummy: received unknown mount event code %i", event);

    }

}

int init_workspace_mount(struct workspace_mount_s *workspace, unsigned int *error)
{

    memset(workspace, 0, sizeof(struct workspace_mount_s));

    workspace->user=NULL;
    workspace->status=0;
    workspace->type=0;

    workspace->mountpoint.path=NULL;
    workspace->mountpoint.len=0;
    workspace->mountpoint.flags=0;
    workspace->mountpoint.refcount=0;

    workspace->syncdate.tv_sec=0;
    workspace->syncdate.tv_nsec=0;

    init_list_header(&workspace->contexes, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_element(&workspace->list, NULL);
    init_list_element(&workspace->list_g, NULL);
    workspace->mountevent=mountevent_dummy;
    workspace->free=free_workspace_mount;

    pthread_mutex_init(&workspace->mutex, NULL);
    workspace->pathmax=512;

    init_workspace_inodes(workspace);
    add_list_element_last(&workspaces, &workspace->list_g);

    return 0;

}

struct workspace_mount_s *get_container_workspace(struct list_element_s *list)
{
    return (list) ? (struct workspace_mount_s *) (((char *) list) - offsetof(struct workspace_mount_s, list)) : NULL;
}

static void notify_VFS_inode_delete(struct workspace_mount_s *workspace, struct inode_s *inode)
{
    struct list_element_s *list=get_list_head(&workspace->contexes, 0);

    if (list) {
	struct service_context_s *context=(struct service_context_s *)(((char *)list) - offsetof(struct service_context_s, wlist));
	struct entry_s *entry=inode->alias;
	struct name_s name={NULL, 0, 0};
	ino_t pino=0;
	ino_t ino=inode->st.st_ino;

	if (entry) {
	    struct entry_s *parent=get_parent_entry(entry);

	    name.name=entry->name.name;
	    name.len=entry->name.len;

	    if (parent) pino=parent->inode->st.st_ino;

	}

	notify_VFS_delete(context->interface.buffer, pino, ino, name.name, name.len);

    }

}

struct inode_s *lookup_workspace_inode(struct workspace_mount_s *workspace, uint64_t ino)
{
    struct workspace_inodes_s *inodes=&workspace->inodes;
    unsigned int hash=ino % WORKSPACE_INODE_HASHTABLE_SIZE;
    struct list_header_s *header=&inodes->hashtable[hash];
    struct list_element_s *list=NULL;
    struct inode_s *inode=NULL;

    read_lock_list_header(header, &inodes->mutex, &inodes->cond);

    list=get_list_head(header, 0);

    while (list) {

	inode=(struct inode_s *)((char *) list - offsetof(struct inode_s, list));

	if (inode->st.st_ino==ino) {

	    /* following code needs a upgrade_read_to_writelock function and that's not ready yet 
		and a swap function */
	    // struct list_element_s *prev=get_next_element(list);

	    // list->count++;

	    // if (prev && list->count > prev->count) {
		//struct list_header_s *h=list->h;

		/* swap */
		//remove_list_element(list);
		//add_list_element_before(h, prev, list);

	    //}

	    break;

	}

	list=get_next_element(list);
	inode=NULL;

    }

    read_unlock_list_header(header, &inodes->mutex, &inodes->cond);
    return inode;

}

static void get_new_ino(struct workspace_inodes_s *inodes, struct inode_s *inode)
{
    pthread_mutex_lock(&inodes->mutex);
    inodes->inoctr++;
    inodes->nrinodes++;
    inode->st.st_ino=inodes->inoctr;
    pthread_mutex_unlock(&inodes->mutex);

}

static void remove_old_ino(struct workspace_inodes_s *inodes, struct inode_s *inode)
{
    pthread_mutex_lock(&inodes->mutex);
    inodes->nrinodes--;
    inode->st.st_ino=0;
    pthread_mutex_unlock(&inodes->mutex);
}

static void _write_inode_workspace_hashtable(struct workspace_mount_s *workspace, struct inode_s *inode, void (*cb)(struct list_header_s *h, struct list_element_s *l))
{
    struct workspace_inodes_s *inodes=&workspace->inodes;
    unsigned int hash=inode->st.st_ino % WORKSPACE_INODE_HASHTABLE_SIZE;
    struct list_header_s *header=&inodes->hashtable[hash];

    write_lock_list_header(header, &inodes->mutex, &inodes->cond);
    (* cb)(header, &inode->list);
    write_unlock_list_header(header, &inodes->mutex, &inodes->cond);

}

static void _add_inode_hash_cb(struct list_header_s *h, struct list_element_s *l)
{
    add_list_element_first(h, l);
}

static void _remove_inode_hash_cb(struct list_header_s *h, struct list_element_s *l)
{
    remove_list_element(l);
}

void add_inode_workspace_hashtable(struct workspace_mount_s *workspace, struct inode_s *inode)
{
    get_new_ino(&workspace->inodes, inode);
    _write_inode_workspace_hashtable(workspace, inode, _add_inode_hash_cb);
}

void remove_inode_workspace_hashtable(struct workspace_mount_s *workspace, struct inode_s *inode)
{
    _write_inode_workspace_hashtable(workspace, inode, _remove_inode_hash_cb);
    remove_old_ino(&workspace->inodes, inode);
}

void add_inode_context(struct service_context_s *context, struct inode_s *inode)
{
    struct entry_s *parent=get_parent_entry(inode->alias);
    struct inode_s *pinode=parent->inode;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

    add_inode_workspace_hashtable(workspace, inode);
    (* pinode->fs->type.dir.use_fs)(context, inode);
    inode->st.st_dev=workspace->inodes.rootinode.st.st_dev;

}

static void inode_2delete_thread(void *ptr)
{
    struct workspace_mount_s *workspace=(struct workspace_mount_s *) ptr;
    struct inode_2delete_s *i2d=NULL;
    size_t hash=0;
    struct inode_s *inode=NULL;
    struct list_element_s *list=NULL;

    geti2d:

    pthread_mutex_lock(&workspace->inodes.mutex);
    list=get_list_head(&workspace->inodes.forget, SIMPLE_LIST_FLAG_REMOVE);

    if (list==NULL) {

	workspace->inodes.thread=0;
	pthread_mutex_unlock(&workspace->inodes.mutex);
	return;

    }

    pthread_mutex_unlock(&workspace->inodes.mutex);

    if (list==NULL) return;
    i2d=(struct inode_2delete_s *)((char *) list - offsetof(struct inode_2delete_s, list));
    inode=lookup_workspace_inode(workspace, i2d->ino);

    if (inode) {
        struct entry_s *entry=inode->alias;
        struct directory_s *directory=get_upper_directory_entry(entry);

        if (i2d->flags & FORGET_INODE_FLAG_FORGET) {

            if (inode->nlookup<=i2d->forget) {

                inode->nlookup=0;

            } else {

                inode->nlookup-=i2d->forget;

            }

        }

        if (directory) {
            unsigned int error=0;
            struct simple_lock_s wlock;

            /* remove entry from directory */

            if (wlock_directory(directory, &wlock)==0) {

                remove_entry_batch(directory, entry, &error);
                unlock_directory(directory, &wlock);

            }

        }

        if ((i2d->flags & FORGET_INODE_FLAG_DELETED) && (inode->flags & INODE_FLAG_DELETED)==0) {

            /* inform VFS, only when the VFS is not the initiator  */

            logoutput("inode_2delete_thread: remote deleted ino %lli name %s", i2d->ino, (entry) ? entry->name.name : "-UNKNOWN-");

	    notify_VFS_inode_delete(workspace, inode);
	    inode->flags |= INODE_FLAG_DELETED;

	}

        /* only when lookup count becomes zero remove it foregood */

        if (inode->nlookup==0) {

	    if (check_service_path_fs(inode)) {

		/* only delete here when on a service related fs: like inodes on a sftp shared directory */

        	logoutput("inode_2delete_thread: remove inode ino %lli name %s", i2d->ino, (entry) ? entry->name.name : "-UNKNOWN-");
		remove_inode_workspace_hashtable(workspace, inode);

        	/* call the inode specific forget which will also release the attached data */

        	(* inode->fs->forget)(inode);

		if (entry) {

            	    entry->inode=NULL;
            	    inode->alias=NULL;
            	    destroy_entry(entry);

		}

        	free_inode(inode);

	    } else {

		logoutput("inode_2delete_thread: inode ino %lli name %s nlookup zero", i2d->ino, (entry) ? entry->name.name : "-UNKNOWN-");

	    }

        }

    }

    free(i2d);
    i2d=NULL;
    goto geti2d;

}

void queue_inode_2forget(struct workspace_mount_s *workspace, ino_t ino, unsigned int flags, uint64_t forget)
{
    struct inode_2delete_s *i2d=malloc(sizeof(struct inode_2delete_s));

    logoutput("queue_inode_2forget: ino %lli forget %lli", ino, forget);

    if (i2d) {

        i2d->ino=ino;
        i2d->flags=flags;
        i2d->forget=forget;
        init_list_element(&i2d->list, &workspace->inodes.forget);

        pthread_mutex_lock(&workspace->inodes.mutex);
	add_list_element_last(&workspace->inodes.forget, &i2d->list);

        if (workspace->inodes.thread==0) {

            workspace->inodes.thread=1;
            work_workerthread(NULL, 0, inode_2delete_thread, (void *) workspace, NULL);

        }

        pthread_mutex_unlock(&workspace->inodes.mutex);

    }

}

void init_workspaces_once()
{
    init_list_header(&workspaces, SIMPLE_LIST_TYPE_EMPTY, 0);
}

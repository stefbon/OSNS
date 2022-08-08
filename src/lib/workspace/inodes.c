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

#include "libosns-basic-system-headers.h"

#include "libosns-misc.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-log.h"

struct inode_2delete_s {
    ino_t				ino;
    unsigned int			flags;
    uint64_t				forget;
    struct list_element_s		list;
};

struct inode_s *lookup_workspace_inode(struct workspace_mount_s *workspace, uint64_t ino)
{

    unsigned int hash=ino % WORKSPACE_INODE_HASHTABLE_SIZE;
    struct list_header_s *header=&workspace->inodes.hashtable[hash];
    struct list_element_s *list=NULL;
    struct inode_s *inode=NULL;

    read_lock_list_header(header);
    list=get_list_head(header, 0);

    while (list) {

	inode=(struct inode_s *)((char *) list - offsetof(struct inode_s, list));

	if (get_ino_system_stat(&inode->stat)==ino) {

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

    read_unlock_list_header(header);
    return inode;

}

static void notify_VFS_inode_delete(struct workspace_mount_s *workspace, struct inode_s *inode)
{
    struct list_element_s *list=get_list_head(&workspace->contexes, 0);

    if (list) {
	struct service_context_s *context=(struct service_context_s *)(((char *)list) - offsetof(struct service_context_s, wlist));
	struct entry_s *entry=inode->alias;
	struct name_s name={NULL, 0, 0};
	ino_t pino=0;
	ino_t ino=get_ino_system_stat(&inode->stat);

	/* gather as much information as possible */

	if (entry) {
	    struct entry_s *parent=get_parent_entry(entry);

	    name.name=entry->name.name;
	    name.len=entry->name.len;
	    if (parent) pino=get_ino_system_stat(&parent->inode->stat);

	}

	notify_VFS_delete(&context->interface, pino, ino, name.name, name.len);

    }

}


static void get_new_ino(struct workspace_mount_s *w, struct inode_s *inode)
{
    uint64_t ino=0;

    if (signal_set_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_INODES)) {

	w->inodes.inoctr++;
	w->inodes.nrinodes++;
	ino=w->inodes.inoctr;
	signal_unset_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_INODES);

    }

    set_ino_system_stat(&inode->stat, ino);
    logoutput_debug("get_new_ino: ino %li", ino);

}

static void remove_old_ino(struct workspace_mount_s *w, struct inode_s *inode)
{

    if (signal_set_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_INODES)) {

	w->inodes.nrinodes--;
	signal_unset_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_INODES);

    }

    set_ino_system_stat(&inode->stat, 0);
}

static void _write_inode_workspace_hashtable(struct workspace_mount_s *w, struct inode_s *inode, void (*cb)(struct list_header_s *h, struct list_element_s *l))
{
    unsigned int hash=get_ino_system_stat(&inode->stat) % WORKSPACE_INODE_HASHTABLE_SIZE;
    struct list_header_s *header=&w->inodes.hashtable[hash];

    write_lock_list_header(header);
    (* cb)(header, &inode->list);
    write_unlock_list_header(header);

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
    get_new_ino(workspace, inode);
    _write_inode_workspace_hashtable(workspace, inode, _add_inode_hash_cb);
}

void remove_inode_workspace_hashtable(struct workspace_mount_s *workspace, struct inode_s *inode)
{
    _write_inode_workspace_hashtable(workspace, inode, _remove_inode_hash_cb);
    remove_old_ino(workspace, inode);
}

void add_inode_context(struct service_context_s *context, struct inode_s *inode)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct system_dev_s dev;

    add_inode_workspace_hashtable(workspace, inode);
    get_dev_system_stat(&workspace->inodes.rootinode.stat, &dev);
    set_dev_system_stat(&inode->stat, &dev);

}

void inherit_fuse_fs_parent(struct service_context_s *context, struct inode_s *inode)
{
    struct directory_s *directory=get_upper_directory_entry(inode->alias);

    if (directory) {
	struct inode_s *pinode=directory->inode;

	(* pinode->fs->type.dir.use_fs)(context, inode);

    }

}

static void inode_2delete_thread(void *ptr)
{
    struct workspace_mount_s *w=(struct workspace_mount_s *) ptr;
    struct inode_2delete_s *i2d=NULL;
    size_t hash=0;
    struct inode_s *inode=NULL;
    struct list_element_s *list=NULL;
    struct list_header_s *header=&w->inodes.forget;

    activethread:

    signal_lock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);
    if (w->inodes.thread) {

	signal_unlock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);
	return;

    }

    w->inodes.thread=1;
    signal_unlock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);

    geti2d:

    write_lock_list_header(header);
    list=get_list_head(header, SIMPLE_LIST_FLAG_REMOVE);

    if (list==NULL) {

	w->inodes.thread=0;
	write_unlock_list_header(header);
	goto finish_and_out;

    }

    write_unlock_list_header(header);

    i2d=(struct inode_2delete_s *)((char *) list - offsetof(struct inode_2delete_s, list));
    inode=lookup_workspace_inode(w, i2d->ino);

    if (inode) {

        if (i2d->flags & FORGET_INODE_FLAG_FORGET) {

	    inode->nlookup-=((inode->nlookup<=i2d->forget) ? inode->nlookup : i2d->forget);

        }

        if ((i2d->flags & FORGET_INODE_FLAG_DELETED) && (inode->flags & INODE_FLAG_DELETED)==0) {

            /* inform VFS, only when the VFS is not the initiator  */

            logoutput("inode_2delete_thread: remote deleted ino %lli", i2d->ino);
	    notify_VFS_inode_delete(w, inode);
	    inode->flags |= INODE_FLAG_DELETED;

	}

        /* only when lookup count becomes zero remove it foregood */

        if ((inode->nlookup==0) || (inode->flags & INODE_FLAG_DELETED)) {

	    logoutput("inode_2delete_thread: remove inode ino %lli (nlookup %li)", i2d->ino, inode->nlookup);

	    if (system_stat_test_ISDIR(&inode->stat)) {
		struct directory_s *directory=get_directory(w, inode, GET_DIRECTORY_FLAG_NOCREATE);

		free_directory(directory);

	    }

	    remove_inode_workspace_hashtable(w, inode);

    	    /* call the inode specific forget which will also release the attached data */

    	    (* inode->fs->forget)(NULL, inode);

	    /* is there an entry??*/

	    if (inode->alias) {
    		struct entry_s *entry=inode->alias;
    		struct directory_s *directory=get_upper_directory_entry(entry);

    		if (directory) {
    		    unsigned int error=0;

		    remove_entry(directory, entry, &error);

		}

        	entry->inode=NULL;
        	inode->alias=NULL;
        	destroy_entry(entry);

	    }

    	    free_inode(inode);

        }

    }

    out:

    free(i2d);
    i2d=NULL;
    goto geti2d;

    finish_and_out:

    signal_lock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);
    w->inodes.thread=0;
    signal_unlock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);

}

void queue_inode_2forget(struct workspace_mount_s *w, ino_t ino, unsigned int flags, uint64_t forget)
{
    struct inode_2delete_s *i2d=malloc(sizeof(struct inode_2delete_s));

    logoutput_debug("queue_inode_2forget: ino %lli flags %u forget %lli", ino, flags, forget);

    if (i2d) {

        i2d->ino=ino;
        i2d->flags=flags;
        i2d->forget=forget;
        init_list_element(&i2d->list, &w->inodes.forget);

	/* add to the "to forget" list */

	write_lock_list_header(&w->inodes.forget);
	add_list_element_last(&w->inodes.forget, &i2d->list);
	write_unlock_list_header(&w->inodes.forget);

	/* start thread if not already running */

	signal_lock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);

        if (w->inodes.thread) {

	    signal_unlock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);
	    return;

	}

        work_workerthread(NULL, 0, inode_2delete_thread, (void *) w, NULL);
        signal_unlock_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD);

    }

}

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
#include "libosns-fuse.h"
#include "libosns-fuse-public.h"
#include "libosns-log.h"

#include "lib/system/path.h"
#include "lib/system/stat.h"


void adjust_pathmax(struct workspace_mount_s *w, unsigned int len)
{
    signal_set_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_PATHMAX);
    if (len>w->pathmax) w->pathmax=len;
    signal_unset_flag(w->signal, &w->status, WORKSPACE_STATUS_LOCK_PATHMAX);
}

unsigned int get_pathmax(struct workspace_mount_s *w)
{
    return w->pathmax;
}

static void init_workspace_inodes(struct workspace_mount_s *workspace)
{
    struct workspace_inodes_s *inodes=&workspace->inodes;
    struct inode_s *inode=&inodes->rootinode;
    struct entry_s *entry=&inodes->rootentry;
    struct system_stat_s *stat=&inode->stat;
    struct system_timespec_s tmp=SYSTEM_TIME_INIT;
    struct directory_s *directory=&inodes->dummy_directory;

    init_inode(inode);
    inode->nlookup=1;
    inode->fs=NULL;
    inode->alias=entry;

    /* root inode stat */

    set_rootstat(&inode->stat);

    /* root entry */

    init_entry(entry, 0);
    entry->inode=inode;
    entry->flags=_ENTRY_FLAG_ROOT;

    /* inodes hashtable */

    for (unsigned int i=0; i<WORKSPACE_INODE_HASHTABLE_SIZE; i++) init_list_header(&inodes->hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    inodes->nrinodes=1;
    inodes->inoctr=FUSE_ROOT_ID;
    init_list_header(&inodes->forget, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&inodes->directories, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&inodes->symlinks, SIMPLE_LIST_TYPE_EMPTY, NULL);

    /* dummy directory */

    init_dummy_directory(directory);
    inode->ptr=&directory->link;
    directory->link.refcount++;

}

struct workspace_mount_s *create_workspace_mount(unsigned char type)
{
    struct workspace_mount_s *workspace=NULL;

    workspace=malloc(sizeof(struct workspace_mount_s));
    if (workspace==NULL) return NULL;
    memset(workspace, 0, sizeof(struct workspace_mount_s));

    workspace->status=0;
    workspace->type=type;
    set_system_time(&workspace->syncdate, 0, 0);
    init_list_header(&workspace->contexes, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&workspace->shared_contexes, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_element(&workspace->list, NULL);
    workspace->pathmax=512;

    init_workspace_inodes(workspace);
    return workspace;
}

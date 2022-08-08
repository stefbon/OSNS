/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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
#include "libosns-datatypes.h"

#include "dentry.h"

void init_data_link(struct data_link_s *link)
{
    link->type=0;
    link->refcount=0;
}

void init_entry(struct entry_s *entry, unsigned int size)
{
    entry->flags=0;
    entry->inode=NULL;
    init_list_element(&entry->list, NULL);

    entry->size=size;

    entry->name.name=&entry->buffer[0];
    entry->name.len=0;
    entry->name.index=0;
}

/*
    allocate an entry and name

    TODO: for the name is now a seperate pointer used entry->name.name, but it would
    be better to use an array for the name
*/

struct entry_s *create_entry(struct name_s *xname)
{
    struct entry_s *entry=NULL;
    unsigned int size=sizeof(struct entry_s) + xname->len + 1;

    entry = malloc(size);

    if (entry) {

	memset(entry, 0, size); /* ensures also a terminating null byte to the name buffer */
	init_entry(entry, size);
	memcpy(&entry->buffer[0], xname->name, xname->len);
	entry->name.name=&entry->buffer[0];
	entry->name.len=xname->len;
	entry->name.index=xname->index;

    }

    return entry;

}

void destroy_entry(struct entry_s *entry)
{
    free(entry);
}

void init_inode(struct inode_s *inode)
{
    struct system_stat_s *stat=&inode->stat;
    struct system_dev_s dev=SYSTEM_DEV_INIT;
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    inode->flags=0;
    init_list_element(&inode->list, NULL);

    inode->alias=NULL;
    inode->nlookup=0;

    set_ino_system_stat(stat, 0);
    set_type_system_stat(stat, 0);
    set_nlink_system_stat(stat, 0);
    set_uid_system_stat(stat, ((uid_t) -1));
    set_gid_system_stat(stat, ((gid_t) -1));
    set_size_system_stat(stat, 0);

    set_blocks_system_stat(stat, 0);
    set_blksize_system_stat(stat, 0);

    set_dev_system_stat(stat, &dev);
    set_rdev_system_stat(stat, &dev);

    set_atime_system_stat(stat, &time);
    set_mtime_system_stat(stat, &time);
    set_btime_system_stat(stat, &time);
    set_ctime_system_stat(stat, &time);

    /* synctime */
    set_system_time(&inode->stime, 0, 0);

    inode->ptr=NULL;
    inode->cache=NULL;
    inode->fs=NULL;

}

void copy_inode_stat(struct inode_s *inode, struct system_stat_s *stat)
{
    memcpy(stat, &inode->stat, sizeof(struct system_stat_s));
}

void fill_inode_stat(struct inode_s *inode, struct system_stat_s *stat)
{

    set_uid_system_stat(&inode->stat, get_uid_system_stat(stat));
    set_gid_system_stat(&inode->stat, get_gid_system_stat(stat));
    copy_atime_system_stat(&inode->stat, stat);
    copy_btime_system_stat(&inode->stat, stat);
    copy_ctime_system_stat(&inode->stat, stat);
    copy_mtime_system_stat(&inode->stat, stat);

}

struct inode_s *create_inode()
{
    struct inode_s *inode = malloc(sizeof(struct inode_s));

    if (inode) {

	memset(inode, 0, sizeof(struct inode_s));
	init_inode(inode);

    }

    return inode;

}

void free_inode(struct inode_s *inode)
{
    free(inode);
}

void log_inode_information(struct inode_s *inode, uint64_t what)
{

    if (what & INODE_INFORMATION_OWNER) logoutput("log_inode_information: owner :%i", get_uid_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_GROUP) logoutput("log_inode_information: owner :%i", get_gid_system_stat(&inode->stat));

    if (what & INODE_INFORMATION_NAME) {
	struct entry_s *entry=inode->alias;

	if (entry) {

	    logoutput("log_inode_information: entry name :%.*s", entry->name.len, entry->name.name);

	} else {

	    logoutput("log_inode_information: no entry");

	}

    }
    if (what & INODE_INFORMATION_NLOOKUP) logoutput("log_inode_information: nlookup :%li", inode->nlookup);
    if (what & INODE_INFORMATION_MODE) logoutput("log_inode_information: mode :%i", get_mode_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_NLINK) logoutput("log_inode_information: nlink :%i", get_nlink_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_SIZE) logoutput("log_inode_information: size :%i", get_size_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_MTIM) logoutput("log_inode_information: mtim %li.%li", get_mtime_sec_system_stat(&inode->stat), get_mtime_nsec_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_CTIM) logoutput("log_inode_information: ctim %li.%li", get_ctime_sec_system_stat(&inode->stat), get_ctime_nsec_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_ATIM) logoutput("log_inode_information: atim %li.%li", get_atime_sec_system_stat(&inode->stat), get_atime_nsec_system_stat(&inode->stat));
    if (what & INODE_INFORMATION_STIM) logoutput("log_inode_information: stim %li.%li", inode->stime.st_sec, inode->stime.st_nsec);

}


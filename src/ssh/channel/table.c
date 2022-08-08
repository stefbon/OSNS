/*
  2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-channel.h"

int channeltable_readlock(struct channel_table_s *table, struct osns_lock_s *rlock)
{
    init_osns_readlock(&table->locking, rlock);
    return osns_lock(rlock);
}

int channeltable_upgrade_readlock(struct channel_table_s *table, struct osns_lock_s *rlock)
{
    return osns_upgradelock(rlock);
}

int channeltable_writelock(struct channel_table_s *table, struct osns_lock_s *wlock)
{
    init_osns_writelock(&table->locking, wlock);
    return osns_lock(wlock);
}

int channeltable_unlock(struct channel_table_s *table, struct osns_lock_s *lock)
{
    return osns_unlock(lock);
}

struct ssh_channel_s *lookup_session_channel_for_payload(struct channel_table_s *table, unsigned int nr, struct ssh_payload_s **p_payload)
{
    unsigned int hashvalue = nr % CHANNELS_TABLE_SIZE;
    struct list_header_s *h=&table->hash[hashvalue];
    struct list_element_s *list=NULL;
    struct ssh_channel_s *channel=NULL;

    read_lock_list_header(h);
    list=get_list_head(h, 0);

    while (list) {

	channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));

	if (channel->local_channel==nr) {

	    queue_ssh_payload_channel(channel, *p_payload);
	    *p_payload=NULL;
	    break;

	}

	list=get_next_element(list);
	channel=NULL;

    }

    read_unlock_list_header(h);
    return channel;
}

struct ssh_channel_s *lookup_session_channel_for_data(struct channel_table_s *table, unsigned int nr, struct ssh_payload_s **p_payload)
{
    unsigned int hashvalue = nr % CHANNELS_TABLE_SIZE;
    struct list_header_s *h=&table->hash[hashvalue];
    struct list_element_s *list=NULL;
    struct ssh_channel_s *channel=NULL;

    read_lock_list_header(h);
    list=get_list_head(h, 0);

    while (list) {

	channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));

	if (channel->local_channel==nr) {

	    (* channel->receive_msg_channel_data)(channel, p_payload);
	    break;

	}

	list=get_next_element(list);
	channel=NULL;

    }

    read_unlock_list_header(h);
    if (*p_payload) free_payload(p_payload);
    return channel;
}

struct ssh_channel_s *lookup_session_channel_for_flag(struct channel_table_s *table, unsigned int nr, unsigned char flag)
{
    unsigned int hashvalue = nr % CHANNELS_TABLE_SIZE;
    struct list_header_s *h=&table->hash[hashvalue];
    struct list_element_s *list=NULL;
    struct ssh_channel_s *channel=NULL;

    read_lock_list_header(h);
    list=get_list_head(h, 0);

    while (list) {

	channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));

	if (channel->local_channel==nr) {

	    signal_set_flag(channel->signal, &channel->flags, flag);
	    break;

	}

	list=get_next_element(list);
	channel=NULL;

    }

    read_unlock_list_header(h);
    return channel;
}

struct ssh_channel_s *lookup_session_channel(struct channel_table_s *table, unsigned int nr)
{
    unsigned int hashvalue = nr % CHANNELS_TABLE_SIZE;
    struct list_header_s *h=&table->hash[hashvalue];
    struct list_element_s *list=NULL;
    struct ssh_channel_s *channel=NULL;

    read_lock_list_header(h);
    list=get_list_head(h, 0);

    while (list) {

	channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));
	if (channel->local_channel==nr) break;
	list=get_next_element(list);
	channel=NULL;

    }

    read_unlock_list_header(h);
    return channel;
}

void init_ssh_channels_table(struct ssh_session_s *session, struct shared_signal_s *signal, unsigned int size)
{
    struct channel_table_s *table=&session->channel_table;

    table->flags=0;
    table->latest_channel=0;
    table->free_channel=(unsigned int) -1;
    table->count=0;
    table->shell=NULL;
    table->signal=((signal) ? signal : get_default_shared_signal());
    table->table_size=size;

    for (unsigned int i=0; i<size; i++) init_list_header(&table->hash[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_osns_locking(&table->locking, 0);

}

void free_ssh_channels_table(struct ssh_session_s *session)
{
    struct channel_table_s *table=&session->channel_table;

    for (unsigned int i=0; i<CHANNELS_TABLE_SIZE; i++) {
	struct list_element_s *list=get_list_head(&table->hash[i], SIMPLE_LIST_FLAG_REMOVE);

	/* remove every element from the individual list headers */

	while (list) {

	    struct ssh_channel_s *channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));
	    free_ssh_channel(&channel);
	    list=get_list_head(&table->hash[i], SIMPLE_LIST_FLAG_REMOVE);

	}

    }

}

struct ssh_channel_s *find_channel(struct ssh_session_s *session, unsigned int type)
{
    struct channel_table_s *table=&session->channel_table;
    struct channellist_head_s *channellist_head=NULL;
    struct ssh_channel_s *channel=NULL;

    for (unsigned int i=0; i<CHANNELS_TABLE_SIZE; i++) {
	struct list_element_s *list=get_list_head(&table->hash[i], 0);

	while (list) {

	    channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));
	    if (channel->type==type) break;
	    list=get_next_element(list);
	    channel=NULL;

	}

    }

    return channel;

}

struct ssh_channel_s *get_next_channel(struct ssh_session_s *session, struct ssh_channel_s *channel)
{
    struct channel_table_s *table=&session->channel_table;
    unsigned int hashvalue = 0;
    struct list_element_s *list=NULL;

    if (channel) {

	list=get_next_element(&channel->list);
	if (list) goto found;
	hashvalue = (channel->local_channel % CHANNELS_TABLE_SIZE) + 1; /* start at next row */

    }

    for (unsigned int i=hashvalue; i<CHANNELS_TABLE_SIZE; i++) {

	list=get_list_head(&table->hash[i], 0);
	if (list) goto found;

    }

    return NULL;

    found:
    return (struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));

}

void table_add_channel(struct ssh_channel_s *channel)
{
    unsigned int hashvalue=0;
    struct ssh_session_s *session=channel->session;
    struct channel_table_s *table=&session->channel_table;
    struct list_header_s *h=NULL;

    if (channel->flags & CHANNEL_FLAG_TABLE) return;

    if (table->latest_channel==0) {

	channel->local_channel=table->latest_channel;

    } else {
	unsigned int start=((table->free_channel < table->latest_channel) ? table->free_channel : (table->latest_channel + 1));
	unsigned int tmp=start;

	/* try a free local channel */

	while (lookup_session_channel(table, tmp)) tmp++;

	channel->local_channel=tmp;

	if (tmp>table->latest_channel) {

	    table->free_channel=(unsigned int) -1; /* max value */
	    table->latest_channel=tmp;

	} else {

	    /* free channel found, find the next free channel if any */

	    tmp++;
	    while (lookup_session_channel(table, tmp)) tmp++;
	    table->free_channel=((tmp<table->latest_channel) ? tmp : (unsigned int) -1);

	}

    }

    hashvalue = (channel->local_channel % CHANNELS_TABLE_SIZE);
    h=&table->hash[hashvalue];
    add_list_element_first(h, &channel->list);
    logoutput("table_add_channel: add channel %u to table", channel->local_channel);
    table->count++;
    channel->flags|=CHANNEL_FLAG_TABLE;

}

void table_remove_channel(struct ssh_channel_s *channel)
{
    struct ssh_session_s *session=channel->session;
    struct channel_table_s *table=&session->channel_table;

    if ((channel->flags & CHANNEL_FLAG_TABLE)==0) return;

    remove_list_element(&channel->list);
    table->count--;
    channel->flags-=CHANNEL_FLAG_TABLE;
    if (channel->local_channel < table->free_channel) table->free_channel=channel->local_channel;

}

int add_channel(struct ssh_channel_s *channel, unsigned int flags)
{
    struct ssh_session_s *session=channel->session;
    struct channel_table_s *table=&session->channel_table;
    int result=-1;
    struct osns_lock_s wlock;

    /* protect the handling of adding/removing channels */

    logoutput("add_channel: add channel to table");

    channeltable_writelock(table, &wlock);
    table_add_channel(channel);
    channeltable_unlock(table, &wlock);

    if (flags & CHANNEL_FLAG_OPEN) {
	unsigned int error=0;

	result=0;

	if ((* channel->start)(channel, &error)==-1) {

	    channeltable_writelock(table, &wlock);
	    table_remove_channel(channel);
	    channeltable_unlock(table, &wlock);
	    result=-1;

	}

    } else {

	result=0;

    }

    return result;

}

void remove_channel(struct ssh_channel_s *channel, unsigned int flags)
{
    struct ssh_session_s *session=channel->session;
    struct channel_table_s *table=&session->channel_table;
    struct osns_lock_s wlock;

    /* protect the handling of adding/removing channels */

    if (flags & (CHANNEL_FLAG_CLIENT_CLOSE | CHANNEL_FLAG_SERVER_CLOSE)) {

	(* channel->close)(channel, flags);

    }

    channeltable_writelock(table, &wlock);
    table_remove_channel(channel);
    channeltable_unlock(table, &wlock);

}

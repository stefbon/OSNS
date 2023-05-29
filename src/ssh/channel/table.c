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

#define SSH_CHANNELS_TABLE_SIZE				64

static struct list_header_s				tablehash[SSH_CHANNELS_TABLE_SIZE];
static unsigned int                                     channelctr = 0;
static struct shared_signal_s                           *tablesignal=NULL;
static unsigned int                                     tablecount = 0;

int lookup_ssh_channel_for_cb(unsigned int lcnr, struct ssh_payload_s **p_payload, int (* cb)(struct ssh_channel_s *c, struct ssh_payload_s **p, void *ptr), void *ptr)
{
    struct ssh_payload_s *payload=*p_payload;
    unsigned int hashvalue = (lcnr % SSH_CHANNELS_TABLE_SIZE);
    struct list_header_s *h=&tablehash[hashvalue];
    struct list_element_s *list=NULL;
    struct ssh_channel_s *channel=NULL;
    int result=-1;

    logoutput_debug("lookup_ssh_channel_for_cb: local channel nr %u payload type %u", lcnr, payload->type);

    read_lock_list_header(h);
    list=get_list_head(h);

    while (list) {

	channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));

	if (channel->lcnr==lcnr) {

	    result=(* cb)(channel, p_payload, ptr);
	    break;

	}

	list=get_next_element(list);
	channel=NULL;

    }

    read_unlock_list_header(h);
    return result;
}

struct doiocb_hlpr_s {
    unsigned char iocbnr;
};

static int doiocb(struct ssh_channel_s *c, struct ssh_payload_s **p, void *ptr)
{
    struct doiocb_hlpr_s *hlpr=(struct doiocb_hlpr_s *) ptr;

    logoutput_debug("doiocb: iocb nr %u", hlpr->iocbnr);
    (* c->iocb[hlpr->iocbnr])(c, p);
    return 0;
}

int lookup_ssh_channel_for_iocb(unsigned int lcnr, struct ssh_payload_s **p_payload, unsigned char iocbnr)
{
    struct doiocb_hlpr_s hlpr={.iocbnr=iocbnr};
    return lookup_ssh_channel_for_cb(lcnr, p_payload, doiocb, (void *)&hlpr);
}

void init_ssh_channels_table(struct shared_signal_s *signal)
{

    if (tablesignal==NULL) {

        tablesignal=((signal) ? signal : get_default_shared_signal());
        for (unsigned int i=0; i<SSH_CHANNELS_TABLE_SIZE; i++) init_list_header(&tablehash[i], SIMPLE_LIST_TYPE_EMPTY, NULL);

    }

}

void clear_ssh_channels_table()
{

    for (unsigned int i=0; i<SSH_CHANNELS_TABLE_SIZE; i++) {
	struct list_element_s *list=remove_list_head(&tablehash[i]);

	/* remove every element from the individual list headers */

	while (list) {

	    struct ssh_channel_s *channel=(struct ssh_channel_s *)(((char *)list) - offsetof(struct ssh_channel_s, list));
	    free_ssh_channel(&channel);
	    list=remove_list_head(&tablehash[i]);

	}

    }

}

struct ssh_channel_s *get_next_ssh_channel(struct ssh_channel_s *channel, unsigned int hashvalue)
{
    struct list_element_s *list=NULL;

    if (hashvalue>=SSH_CHANNELS_TABLE_SIZE) return NULL;
    list=(channel ? get_next_element(&channel->list) : get_list_head(&tablehash[hashvalue]));

    return (list ? (struct ssh_channel_s *)((char *) list - offsetof(struct ssh_channel_s, list)) : NULL);

}

void ssh_channel_table_writelock(unsigned int row)
{

    if (row<SSH_CHANNELS_TABLE_SIZE) {
        struct list_header_s *h=&tablehash[row];

        write_lock_list_header(h);

    }

}

void ssh_channel_table_writeunlock(unsigned int row)
{

    if (row<SSH_CHANNELS_TABLE_SIZE) {
        struct list_header_s *h=&tablehash[row];

        write_unlock_list_header(h);

    }

}

static void table_add_ssh_channel(struct ssh_channel_s *channel)
{
    unsigned int hashvalue=0;
    struct list_header_s *h=NULL;

    if (channel->flags & SSH_CHANNEL_FLAG_TABLE) return;

    signal_lock(tablesignal);
    channel->lcnr=channelctr;
    channelctr++;
    tablecount++;
    signal_unlock(tablesignal);

    hashvalue = (channel->lcnr % SSH_CHANNELS_TABLE_SIZE);
    h=&tablehash[hashvalue];

    write_lock_list_header(h);
    add_list_element_first(h, &channel->list);
    write_unlock_list_header(h);

    channel->flags|=SSH_CHANNEL_FLAG_TABLE;

}

static void table_remove_ssh_channel(struct ssh_channel_s *channel, unsigned char locked)
{
    unsigned int hashvalue = (channel->lcnr % SSH_CHANNELS_TABLE_SIZE);
    struct list_header_s *h=&tablehash[hashvalue];

    if ((channel->flags & SSH_CHANNEL_FLAG_TABLE)==0) return;

    if (locked==0) write_lock_list_header(h);
    remove_list_element(&channel->list);
    if (locked==0) write_unlock_list_header(h);

    signal_lock(tablesignal);
    tablecount--;
    signal_unlock(tablesignal);

    channel->flags &= ~SSH_CHANNEL_FLAG_TABLE;

}

int add_ssh_channel(struct ssh_channel_s *channel, unsigned int flags)
{
    int result=-1;
    unsigned int error=0;

    table_add_ssh_channel(channel);

    if ((* channel->start)(channel, NULL)==-1) {

	table_remove_ssh_channel(channel, 0);

    } else {

	result=0;

    }

    return result;

}

void remove_ssh_channel(struct ssh_channel_s *channel, unsigned int flags, unsigned char locked)
{
    if (flags & (SSH_CHANNEL_FLAG_CLIENT_CLOSE | SSH_CHANNEL_FLAG_SERVER_CLOSE)) (* channel->close)(channel, flags);
    table_remove_ssh_channel(channel, locked);
}

unsigned int get_ssh_channels_tablesize()
{
    return SSH_CHANNELS_TABLE_SIZE;
}

struct ssh_channel_s *walk_ssh_channels(int (* cb)(struct ssh_channel_s *c, void *ptr), void *ptr)
{
    unsigned int hashvalue=0;
    struct ssh_channel_s *channel=NULL;

    while (hashvalue < get_ssh_channels_tablesize()) {

        ssh_channel_table_writelock(hashvalue);

        channel=get_next_ssh_channel(NULL, hashvalue);

	while (channel) {
	    struct ssh_channel_s *next=get_next_ssh_channel(channel, hashvalue);

            if ((* cb)(channel, ptr)) break;
	    channel=next;

	}

	ssh_channel_table_writeunlock(hashvalue);
	if (channel) break;
	hashvalue++;

    }

    return channel;

}
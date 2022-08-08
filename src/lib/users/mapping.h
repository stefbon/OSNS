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

#ifndef LIB_USERS_MAPPING_H
#define LIB_USERS_MAPPING_H

#include "libosns-datatypes.h"
#include "libosns-sl.h"

#include "getent.h"
#include "mapping.h"

/*

    MAPPING of user- and groupnames in a posix environment

    User- and groupnames are exchanged by client and server in the attributes structure, with for example
    a getstat call.

    It's good to note that (at least with a FUSE fs and SFTP) the server can export all kinds of user- and groupnames
    (in a reply to getstat, lookup and readdir), the client will only export one user and that's user for who the
    connection is made (in a setstat).

    In the general case these names may not exist on the other machine. For some accounts though you can be sure
    they exist and map:

    client                                              server
    ======                                              ======

    local user for who the connection is created        remote user used to connect to server
    and who's keys and/or passwords are used            this may be a different user(name) than the user on the client

    root (defined by uid 0)                             root (defined by uid 0)

    nobody (defined by (uid_t) -1)			nobody (defined by (uid_t) -1)


    If you are strict, you cannot map other user/groupnames on server and client, not even when their names match. Names ot does not
    reckognize are mapped to the local "unknown" user/group.
    You can characterize this situation by (NONSHARED | STRICT).

    When you are less strict, by for example you know the user- and groupnames match, and map, the client accepts the names it does reckognize
    directly by doing a standard nss lookup.
    You can characterize this situation by (NONSHARED).

    It's possible that client and server share a user- and group nss database like OpenLDAP and (Microsoft) Active Directory. In this case
    (which looks very much like the NONSHARED case) only for the obvious names mapping is done, any other is done via a nss name lookup. It's
    important that the remote shared nss database is trusted by the client.
    You can characterize this situation by (SHARED | USERDB).

    With both situations there is no domain part in the user- and groupname.

    An other variant is that the server and client use user- and groupnames in an email format like "user@domain.net", which are specific
    for a server in a certain domain. This maybe very usefull in an environment where the client to at the same time connected to different
    servers in different domains.

    The problem here is that for integration with a platform like Linux, those email style names are not allowed. Also a set of user and groupnames
    like this may have a meaning within the context of the server, it's only that for the client during the client is connected to the server.
    To make this work the following can help:
    - an username like user@domain.net is not allowed under Linux, but domain.net\user is,
    which is used by Winbind to make Linux integrate in a Windows SMB/AD evironment.
    So users and groups will show up as:

    domain.net\user
    domain.net\group

    It's up to clients software, like a gui filemanager, to convert this into a user@domain.net format.

    - because of the temporary character of this set of user- and groupnames, the client can use a range of the available
    uids and gids. With Linux for uids and gids 65535 are available, 0-999 are used for system accounts, 1000- are for local users.
    Assume that a domain will have max 5000 usernames, the client can for example have the following

    example.net\j.smith:x:5000:5000:/dev/null:/usr/bin/false
    example.net\r.wells:x:5001:5000:/dev/null:/usr/bin/false
    ....
    ...
    ..

    and groups:

    example.net\office:x:5000:
    example.net\development:x:5001:example.net\r.wells
    ....
    ...
    ..

    - because the (local) user and one of the new users it has been mapped to are different, the default permissions check,
    (= using unix mode and the owner you are and the groups you are in) does not work as it should. There for the FUSE call
    access has to be used (in stead of the checking via the rwx permission bits in the owner-group-others triple).

    - for the client to make use of the mapping this way, a cache is required:

    client                                                                       server
    ======                                                                       ======

    local user lookup using  <-> osns client cache               <===>           custom db with user and group
    nss libraries                for example.net from                            accounts 
    for domain example.net       server.example.net

    The server will only give the custom db for example.net to the client if the client has provided a strong hostkey.

    - the server has to be cautious giving this information: only hosts which can provide a valid (public) hostkey 


*/

struct net_idmapping_s;

#define NET_ENTITY_FLAG_USER				1
#define NET_ENTITY_FLAG_GROUP				2
#define NET_ENTITY_FLAG_NODOMAIN			4

struct net_entity_s {
    unsigned int					flags;
    struct _network_name_s {
	struct ssh_string_s 				name;
	struct ssh_string_s				domain;
	unsigned int					id;
    } net;
    union _local_id_u {
	uid_t						uid;
	gid_t						gid;
    } local;
};

struct net_entlookup_s {
    void						(* lookup_user)(struct net_idmapping_s *m, struct net_entity_s *u, unsigned int *e);
    void						(* lookup_group)(struct net_idmapping_s *m, struct net_entity_s *g, unsigned int *e);
};

#define NET_IDMAPPING_FLAG_INIT				1 << 0
#define NET_IDMAPPING_FLAG_SETUP			1 << 1
#define NET_IDMAPPING_FLAG_STARTED			1 << 2
#define NET_IDMAPPING_FLAG_COMPLETE			1 << 3

#define NET_IDMAPPING_FLAG_CLIENT			1 << 8
#define NET_IDMAPPING_FLAG_SERVER			1 << 9

#define NET_IDMAPPING_FLAG_SHARED			1 << 10
#define NET_IDMAPPING_FLAG_NSS_USERDB			1 << 11
#define NET_IDMAPPING_FLAG_DOMAINDB			1 << 12

#define NET_IDMAPPING_FLAG_MAPBYID			1 << 20
#define NET_IDMAPPING_FLAG_MAPBYNAME			1 << 21

#define NET_IDMAPPING_FLAG_NONSTRICT			1 << 22

#define NET_IDMAPPING_ALLFLAGS	(NET_IDMAPPING_FLAG_SHARED | NET_IDMAPPING_FLAG_NSS_USERDB | NET_IDMAPPING_FLAG_DOMAINDB | NET_IDMAPPING_FLAG_MAPBYID | NET_IDMAPPING_FLAG_MAPBYNAME | NET_IDMAPPING_FLAG_NONSTRICT)

/* callbacks to translate the protocol user to a local one and vice versa */

struct net_mapcb_s {
    void						(* get_user_p2l)(struct net_idmapping_s *m, struct net_entity_s *u);
    void						(* get_group_p2l)(struct net_idmapping_s *m, struct net_entity_s *g);
    void						(* get_user_l2p)(struct net_idmapping_s *m, struct net_entity_s *u);
    void						(* get_group_l2p)(struct net_idmapping_s *m, struct net_entity_s *g);
};

struct net_idmapping_s {
    unsigned int					flags;
    pthread_mutex_t					*mutex;
    struct passwd					*pwd;
    struct ssh_string_s					getent_su; /* getent of connecting user on server */
    struct getent_fields_s				su; /* fields read from getent user on server */
    struct ssh_string_s					getent_sg; /* getent of group of connecting user on server */
    struct getent_fields_s				sg; /* fields as read om getent_sg */
    uid_t						unknown_uid;
    gid_t						unknown_gid;
    struct net_mapcb_s					mapcb;
    struct net_entlookup_s				lookup;
    int							(* setup)(struct net_idmapping_s *mapping, unsigned int flags);
};

/*
    void						(* add_net_entity)(struct net_client_idmapping_s *mapping, struct net_entity_s *ent);
    int							(* get_system_getents_file)(struct net_client_idmapping_s *mapping, const char *target, struct ssh_string_s *f);
    struct net_userscache_s				*cache;
};
*/

/* prototypes */

void init_net_idmapping(struct net_idmapping_s *m, struct passwd *pwd);
void free_net_idmapping(struct net_idmapping_s *m);

#endif

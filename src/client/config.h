/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017  Stef Bon <stefbon@gmail.com>

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

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include "arguments.h"

/* FUSE */

#define OSNS_OPTIONS_FUSE_ATTR_TIMEOUT					1.0
#define OSNS_OPTIONS_FUSE_ENTRY_TIMEOUT					1.0
#define OSNS_OPTIONS_FUSE_NEGATIVE_TIMEOUT				1.0
#define OSNS_OPTIONS_FUSE_MAXREAD					8192

/* NETWORK */

#define OSNS_OPTIONS_NETWORK_SHOW_ICON					(1 << 0)
#define OSNS_OPTIONS_NETWORK_SHOW_NETWORKNAME				(1 << 1)
#define OSNS_OPTIONS_NETWORK_SHOW_DOMAINNAME				(1 << 2)
#define OSNS_OPTIONS_NETWORK_HIDE_DOTFILES				(1 << 3)

#define OSNS_OPTIONS_NETWORK_DEFAULT_NETWORK_NAME			(1 << 4)
#define OSNS_OPTIONS_NETWORK_NAME					"Open Secure Network"


//#define OSNS_OPTIONS_NETWORK_ENABLE_SFTP				(1 << 1)
//#define OSNS_OPTIONS_NETWORK_ENABLE_SMB					(1 << 2)
//#define OSNS_OPTIONS_NETWORK_ENABLE_NFS					(1 << 3)
//#define OSNS_OPTIONS_NETWORK_ENABLE_WEBDAV				(1 << 4)

/* SSH */

/* the db/file on this machine with trusted host keys
    like /etc/ssh/known_hosts and ~/.ssh/known_hosts for openssh */

#define OSNS_OPTIONS_SSH_TRUSTDB_NONE					0
#define OSNS_OPTIONS_SSH_TRUSTDB_OPENSSH				1

/* other sources of trusted keys? */

/* SFTP */

#define OSNS_OPTIONS_SFTP_PACKET_MAXSIZE				8192

#define OSNS_OPTIONS_SFTP_HOME_USE_REMOTENAME				1
#define OSNS_OPTIONS_SFTP_SYMLINKS_DISABLE				2
#define OSNS_OPTIONS_SFTP_SYMLINK_ALLOW_PREFIX				4
#define OSNS_OPTIONS_SFTP_SYMLINK_ALLOW_CROSS_INTERFACE			8

struct fuse_client_options_s {
    unsigned int			flags;
    struct system_timespec_s		attr_timeout;
    struct system_timespec_s		entry_timeout;
    struct system_timespec_s		neg_timeout;
    unsigned int			maxread;
};

struct network_client_options_s {
    unsigned int			flags;
    char				*name;
};

struct ssh_client_options_s {
    unsigned int			flags;
};

struct sftp_client_options_s {
    unsigned int			flags;
    unsigned int			maxpacketsize;
};

struct client_options_s {
    char				*runpath;
    struct fuse_client_options_s	fuse;
    struct network_client_options_s	network;
    struct ssh_client_options_s		ssh;
    struct sftp_client_options_s	sftp;
};

/* Prototypes */

void set_default_options(struct client_options_s *options);
int read_configfile(struct client_options_s *options, struct client_arguments_s *arg);

#endif

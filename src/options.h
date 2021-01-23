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
#ifndef _OSNS_OPTIONS_H
#define _OSNS_OPTIONS_H

#include "misc.h"

#define _OPTIONS_MAIN_CONFIGFILE 					"/etc/osns/options"
#define _OPTIONS_MAIN_SOCKET 						"/run/osns/sock"
#define _OPTIONS_MAIN_FLAG_SERVER					1

/* FUSE */

#define _OPTIONS_FUSE_ATTR_TIMEOUT					1.0
#define _OPTIONS_FUSE_ENTRY_TIMEOUT					1.0
#define _OPTIONS_FUSE_NEGATIVE_TIMEOUT					1.0

#define _OPTIONS_FUSE_FLAG_NETWORK_IGNORE_SERVICE			1

/* NETWORK */

#define _OPTIONS_NETWORK_FLAG_SPECIALFILE_READDIR			1

#define _OPTIONS_NETWORK_DISCOVER_METHOD_AVAHI				1
#define _OPTIONS_NETWORK_DISCOVER_METHOD_FILE				2
#define _OPTIONS_NETWORK_DISCOVER_STATIC_FILE_DEFAULT			"/etc/osns/network.services"

#define _OPTIONS_NETWORK_ICON_HIDE					0
#define _OPTIONS_NETWORK_ICON_SHOW					1
#define _OPTIONS_NETWORK_ICON_OVERRULE					2

#define _OPTIONS_NETWORK_ENABLE_SSH					1
#define _OPTIONS_NETWORK_ENABLE_SMB					2
#define _OPTIONS_NETWORK_ENABLE_NFS					4
#define _OPTIONS_NETWORK_ENABLE_WEBDAV					8

/* SSH */

#define _OPTIONS_SSH_FLAG_SUPPORT_EXT_INFO				1
#define _OPTIONS_SSH_FLAG_SUPPORT_CERTIFICATES				2

#define _OPTIONS_SSH_CIPHER_NONE					1
#define _OPTIONS_SSH_CIPHER_3DES_CBC					2
#define _OPTIONS_SSH_CIPHER_AES128_CBC					3
#define _OPTIONS_SSH_CIPHER_AES128_CTR					4
#define _OPTIONS_SSH_CIPHER_AES192_CBC					5
#define _OPTIONS_SSH_CIPHER_AES192_CTR					6
#define _OPTIONS_SSH_CIPHER_AES256_CBC					7
#define _OPTIONS_SSH_CIPHER_AES256_CTR					8
#define _OPTIONS_SSH_CIPHER_CHACHA20_POLY1305_OPENSSH_COM		9

#define _OPTIONS_SSH_HMAC_NONE						1
#define _OPTIONS_SSH_HMAC_SHA1						2
#define _OPTIONS_SSH_HMAC_SHA256					3
#define _OPTIONS_SSH_HMAC_MD5						4

/* TODO: extra options to specify the minimum requirements like nr of bits with rsa for ex. */

#define _OPTIONS_SSH_PUBKEY_RSA						1
#define _OPTIONS_SSH_PUBKEY_DSS						2
#define _OPTIONS_SSH_PUBKEY_ED25519					3

#define _OPTIONS_SSH_COMPRESS_NONE					1
#define _OPTIONS_SSH_COMPRESS_ZLIB					2

/* TODO: extra options to specify the minimum requirements like nr of bits with classic dh */

#define _OPTIONS_SSH_KEYX_DH						1
#define _OPTIONS_SSH_KEYX_ECDH						2
#define _OPTIONS_SSH_KEYX_NONE						3

#define _OPTIONS_SSH_CERTIFICATE_RSA_CERT_V01_OPENSSH_COM		1
#define _OPTIONS_SSH_CERTIFICATE_DSS_CERT_V01_OPENSSH_COM		2
#define _OPTIONS_SSH_CERTIFICATE_ED25519_CERT_V01_OPENSSH_COM		3

#define _OPTIONS_SSH_EXTENSION_SERVER_SIG_ALGS				1
#define _OPTIONS_SSH_EXTENSION_DELAY_COMPRESSION			2
#define _OPTIONS_SSH_EXTENSION_NO_FLOW_CONTROL				3
#define _OPTIONS_SSH_EXTENSION_ELEVATION				4

#define _OPTIONS_SSH_TIMEOUT_INIT_DEFAULT				2
#define _OPTIONS_SSH_TIMEOUT_SESSION_DEFAULT				2
#define _OPTIONS_SSH_TIMEOUT_EXEC_DEFAULT				2
#define _OPTIONS_SSH_TIMEOUT_USERAUTH_DEFAULT				12

#define _OPTIONS_SSH_BACKEND_OPENSSH					1

#define _OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_PASSWORD			1
#define _OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_PUBLICKEY			2
#define _OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_HOSTBASED			4


/* other TODO :
    _OPTIONS_SSH_BACKEND_GPGME
*/

/* the db/file on this machine with trusted host keys
    like /etc/ssh/known_hosts and ~/.ssh/known_hosts for openssh */

#define _OPTIONS_SSH_TRUSTDB_NONE					0
#define _OPTIONS_SSH_TRUSTDB_OPENSSH					1

/* other sources of trusted hostkeys? */

/* SFTP */

#define _OPTIONS_SFTP_PACKET_MAXSIZE					8192
#define _OPTIONS_SFTP_USERMAPPING_NONE					1
#define _OPTIONS_SFTP_USERMAPPING_MAP					2
#define _OPTIONS_SFTP_USERMAPPING_FILE					3
#define _OPTIONS_SFTP_USERMAPPING_DEFAULT				_OPTIONS_SFTP_USERMAPPING_MAP

#define _OPTIONS_SFTP_NETWORK_NAME_DEFAULT				"Open Secure Network"

#define _OPTIONS_SFTP_FLAG_SHOW_NETWORKNAME				1
#define _OPTIONS_SFTP_FLAG_SHOW_DOMAINNAME				2
#define _OPTIONS_SFTP_FLAG_HOME_USE_REMOTENAME				4
#define _OPTIONS_SFTP_FLAG_SYMLINKS_DISABLE				8
#define _OPTIONS_SFTP_FLAG_SYMLINK_ALLOW_PREFIX				16
#define _OPTIONS_SFTP_FLAG_SYMLINK_ALLOW_CROSS_INTERFACE		32
#define _OPTIONS_SFTP_FLAG_HIDE_DENTRIES				64

#define _OPTIONS_SFTP_HIDE_FLAG_DOTFILE					1

/* NFS */

#define _OPTIONS_NFS_PACKET_MAXSIZE					8192

#define _OPTIONS_NFS_NETWORK_NAME_DEFAULT				"Network File System"

#define _OPTIONS_NFS_FLAG_SHOW_DOMAINNAME				1
#define _OPTIONS_NFS_FLAG_HOME_USE_REMOTENAME				2
#define _OPTIONS_NFS_FLAG_USE_VERSION_4					4

/* USER*/

#define _OPTIONS_USER_NETWORK_MOUNT_TEMPLATE_DEFAULT			"/run/network/$USER/fs"
#define _OPTIONS_USER_FLAG_NETWORK_GID_MIN				1
#define _OPTIONS_USER_FLAG_NETWORK_GID_PARTOF				2
#define _OPTIONS_USER_FLAG_NETWORK_GID_DEFAULT				_OPTIONS_USER_FLAG_NETWORK_GID_PARTOF
#define _OPTIONS_USER_NETWORK_GID					1000

struct ssh_options_s {
    unsigned int			flags;
    unsigned int			extensions;
    unsigned int			cipher;
    unsigned int			hmac;
    unsigned int			pubkey;
    unsigned int			certificate;
    unsigned int			compression;
    unsigned int			keyx;
    unsigned int 			timeout_init;
    unsigned int 			timeout_session;
    unsigned int 			timeout_exec;
    unsigned int			timeout_userauth;
    unsigned int 			backend;
    unsigned int 			trustdb;
    unsigned int			required_authmethods;
};

struct sftp_options_s {
    unsigned int 			flags;
    unsigned int			hideflags;
    char 				*usermapping_user_unknown;
    char 				*usermapping_user_nobody;
    unsigned int			packet_maxsize;
    unsigned char			usermapping_type;
    char				*usermapping_file;
    char				*network_name;
};

struct nfs_options_s {
    unsigned int 			flags;
    unsigned int			packet_maxsize;
    char				*network_name;
};

struct network_options_s {
    unsigned int 			flags;
    unsigned int			services;
    unsigned int			discover;
    struct pathinfo_s 			discover_static_file;
    char				*path_icon_network;
    char				*path_icon_domain;
    char				*path_icon_server;
    char				*path_icon_share;
    unsigned int			network_icon;
    unsigned int			domain_icon;
    unsigned int			server_icon;
    unsigned int			share_icon;
};

struct fuse_options_s {
    unsigned int			flags;
    struct timespec			timeout_attr;
    struct timespec			timeout_entry;
    struct timespec			timeout_negative;
};

struct user_options_s {
    unsigned char			flags;
    struct pathinfo_s			network_mount_template;
    gid_t				network_mount_group;
};

struct fs_options_s {
    unsigned int			flags;
    struct pathinfo_s			configfile;
    struct pathinfo_s			socket;
    struct pathinfo_s			discovermap;
    struct fuse_options_s		fuse;
    struct network_options_s		network;
    struct ssh_options_s		ssh;
    struct sftp_options_s		sftp;
    struct nfs_options_s		nfs;
    struct user_options_s		user;
};

// Prototypes

int parse_arguments(int argc, char *argv[], unsigned int *error);
void free_options();

#endif

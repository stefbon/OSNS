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

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <grp.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "main.h"
#include "misc.h"
#include "options.h"
#include "log.h"

extern struct fs_options_s fs_options;

static void print_help(const char *progname) {

    fprintf(stdout, "General options:\n");
    fprintf(stdout, "    --help                print help\n");
    fprintf(stdout, "    --version             print version\n");
    fprintf(stdout, "    --configfile=PATH     (default: %s)\n" , _OPTIONS_MAIN_CONFIGFILE);

    fprintf(stdout, "\n");
    fprintf(stdout, "\n");

}

static void print_version()
{
    printf("fs-workspace version %s\n", PACKAGE_VERSION);
}

void parse_options_commalist(char *list, unsigned int size, void (* cb)(char *entry, void *ptr), void *ptr)
{
    int left = (size>0) ? size : strlen(list);
    char *sep=NULL;
    char *start=list;

    finditem:

    sep=memchr(start, ',', left);
    if (sep) *sep='\0';

    (* cb)(start, ptr);

    if (sep) {

	*sep=',';
	left-=(sep - start);
	start=sep+1;
	if (left>0) goto finditem;

    }

}

static void parse_network_discover_option_cb(char *name, void *ptr)
{
    struct network_options_s *network=(struct network_options_s *) ptr;

    if (strcmp(name, "avahi")==0) {

	network->flags |= _OPTIONS_NETWORK_DISCOVER_METHOD_AVAHI;

    } else if (strcmp(name, "static-file")==0) {

	network->flags |= _OPTIONS_NETWORK_DISCOVER_METHOD_FILE;

    }

}

static void parse_network_services_option_cb(char *name, void *ptr)
{
    struct network_options_s *network=(struct network_options_s *) ptr;

    if (strcmp(name, "ssh")==0) {

	network->services |= _OPTIONS_NETWORK_ENABLE_SSH;

    } else if (strcmp(name, "smb")==0) {

	network->flags |= _OPTIONS_NETWORK_ENABLE_SMB;

    } else if (strcmp(name, "nfs")==0) {

	network->flags |= _OPTIONS_NETWORK_ENABLE_NFS;

    } else if (strcmp(name, "webdav")==0) {

	network->flags |= _OPTIONS_NETWORK_ENABLE_WEBDAV;

    }

}

static void parse_ssh_userauth_methods_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "password")==0) {

	ssh->required_authmethods |= _OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_PASSWORD;

    } else if (strcmp(name, "publickey")==0) {

	ssh->required_authmethods |= _OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_PUBLICKEY;

    } else if (strcmp(name, "hostbased")==0) {

	ssh->required_authmethods |= _OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_HOSTBASED;

    }

}

static void parse_ssh_extensions_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "server-sig-algs")==0) {

	ssh->extensions|=(1 << (_OPTIONS_SSH_EXTENSION_SERVER_SIG_ALGS - 1));

    } else if (strcmp(name, "delay-compression")==0) {

	ssh->extensions|=(1 << (_OPTIONS_SSH_EXTENSION_DELAY_COMPRESSION - 1));

    } else if (strcmp(name, "no-flow-control")==0) {

	ssh->extensions|=(1 << (_OPTIONS_SSH_EXTENSION_NO_FLOW_CONTROL - 1));

    } else if (strcmp(name, "elevation")==0) {

	ssh->extensions|=(1 << (_OPTIONS_SSH_EXTENSION_ELEVATION - 1));

    }

}

static void parse_ssh_ciphers_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "3des-cbc")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_3DES_CBC - 1));

    } else if (strcmp(name, "aes128-cbc")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_AES128_CBC - 1));

    } else if (strcmp(name, "aes128-ctr")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_AES128_CTR - 1));

    } else if (strcmp(name, "aes192-cbc")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_AES192_CBC - 1));

    } else if (strcmp(name, "aes192-ctr")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_AES192_CTR - 1));

    } else if (strcmp(name, "aes256-cbc")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_AES256_CBC - 1));

    } else if (strcmp(name, "aes256-ctr")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_AES256_CTR - 1));

    } else if (strcmp(name, "chacha20-poly1305@openssh.com")==0) {

	ssh->cipher|=(1 << (_OPTIONS_SSH_CIPHER_CHACHA20_POLY1305_OPENSSH_COM - 1));

    }

}

static void parse_ssh_hmac_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "hmac-sha1")==0) {

	ssh->hmac|=(1 << (_OPTIONS_SSH_HMAC_SHA1 - 1));

    } else if (strcmp(name, "hmac-sha256")==0) {

	ssh->hmac|=(1 << (_OPTIONS_SSH_HMAC_SHA256 - 1));

    } else if (strcmp(name, "hmac-md5")==0) {

	ssh->hmac|=(1 << (_OPTIONS_SSH_HMAC_MD5 - 1));

    }

}

static void parse_ssh_pubkey_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "rsa")==0) {

	ssh->pubkey|=(1 << (_OPTIONS_SSH_PUBKEY_RSA - 1));

    } else if (strcmp(name, "dss")==0) {

	ssh->pubkey|=(1 << (_OPTIONS_SSH_PUBKEY_DSS - 1));

    } else if (strcmp(name, "ed25519")==0) {

	ssh->pubkey|=(1 << (_OPTIONS_SSH_PUBKEY_ED25519 - 1));

    }

}

static void parse_ssh_compress_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "zlib")==0) {

	ssh->keyx|=(1 << (_OPTIONS_SSH_COMPRESS_ZLIB - 1));

    }

}

static void parse_ssh_keyx_cb(char *name, void *ptr)
{
    struct ssh_options_s *ssh=(struct ssh_options_s *) ptr;

    if (strcmp(name, "dh")==0) {

	ssh->keyx|=(1 << (_OPTIONS_SSH_KEYX_DH - 1));

    } else if (strcmp(name, "ecdh")==0) {

	ssh->keyx|=(1 << (_OPTIONS_SSH_KEYX_ECDH - 1));

    } else if (strcmp(name, "none")==0) {

	ssh->keyx|=(1 << (_OPTIONS_SSH_KEYX_NONE - 1));

    }

}

static void convert_double_to_timespec(struct timespec *timeout, double tmp)
{
    timeout->tv_sec=(uint64_t) tmp;
    timeout->tv_nsec=(uint64_t) ((tmp - timeout->tv_sec) * 1000000000);
}

static void parse_fuse_timeout_option(struct timespec *timeout, char *value)
{
    double tmp=strtod(value, NULL);
    convert_double_to_timespec(timeout, tmp);
}

static int read_config(char *path)
{
    FILE *fp;
    int result=0;
    char *line=NULL;
    char *sep;
    size_t size=0;
    unsigned int len=0;

    fprintf(stdout, "read_config: open %s\n", path);

    fp=fopen(path, "r");
    if ( fp ==NULL ) return 0;

    while (getline(&line, &size, fp)>0) {

	sep=memchr(line, '\n', size);
	if (sep) *sep='\0';
	len=strlen(line);
	if (len==0) continue;

	sep=memchr(line, '=', len);

	if (sep) {
	    char *option=line;
	    char *value=sep + 1;

	    *sep='\0';
	    skip_trailing_spaces(option, strlen(option), SKIPSPACE_FLAG_REPLACEBYZERO);
	    convert_to(option, UTILS_CONVERT_SKIPSPACE | UTILS_CONVERT_TOLOWER);
	    len=strlen(value);

	    skip_heading_spaces(value, len);
	    fprintf(stdout, "read_config: found %s:%s\n", option, value);

	    /* MAIN */

	    if (strlen(option)==0 || option[0]== '#') continue;

	    if ( strcmp(option, "main.server.socket")==0 ) {

		if ( len>0 ) {

		    fs_options.socket.path=strdup(value); /* check it does exist is later */

		    if ( ! fs_options.socket.path) {

			result=-1;
			fprintf(stderr, "read_config: option %s with value %s cannot be parsed (error %i). Cannot continue.\n", option, value, errno);
			goto out;

		    } else {

			fs_options.socket.len=strlen(fs_options.socket.path);
			fs_options.socket.flags=PATHINFO_FLAGS_ALLOCATED;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    /* FUSE */

	    } else if (strcmp(option, "fuse.timeout_attr")==0) {

		if (len>0) parse_fuse_timeout_option(&fs_options.fuse.timeout_attr, value);

	    } else if (strcmp(option, "fuse.timeout_entry")==0) {

		if (len>0) parse_fuse_timeout_option(&fs_options.fuse.timeout_entry, value);

	    } else if (strcmp(option, "fuse.timeout_negative")==0) {

		if (len>0) parse_fuse_timeout_option(&fs_options.fuse.timeout_negative, value);

	    /* NETWORK */

	    } else if (strcmp(option, "network.discover_methods")==0) {

		if ( len>0 ) {

		    parse_options_commalist(value, len, parse_network_discover_option_cb, (void *) &fs_options.network);

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "network.services")==0) {

		if ( len>0 ) {

		    parse_options_commalist(value, len, parse_network_services_option_cb, (void *) &fs_options.network);

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if ( strcmp(option, "network.discover_static_file")==0 ) {

		if ( len>0 ) {
		    struct pathinfo_s *pathinfo=&fs_options.network.discover_static_file;

		    pathinfo->path=strdup(value); /* check it does exist is later */

		    if (pathinfo->path) {

			pathinfo->len=len;
			pathinfo->flags=PATHINFO_FLAGS_ALLOCATED;

		    } else {

			result=-1;
			fprintf(stderr, "read_config: option %s with value %s cannot be parsed (error %i). Cannot continue.\n", option, value, errno);
			goto out;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    /* SSH */

	    } else if (strcmp(option, "ssh.support.ext-info")==0) {

		if ( len>0 ) {

		    if (strcmp(value, "1")==0 || strcmp(value, "yes")==0) {

			fs_options.ssh.flags |= _OPTIONS_SSH_FLAG_SUPPORT_EXT_INFO;

		    } else if (strcmp(value, "0")==0 || strcmp(value, "no")==0) {

			if (fs_options.ssh.flags & _OPTIONS_SSH_FLAG_SUPPORT_EXT_INFO) fs_options.ssh.flags -= _OPTIONS_SSH_FLAG_SUPPORT_EXT_INFO;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "ssh.support.certificates")==0) {

		if ( len>0 ) {

		    if (strcmp(value, "1")==0 || strcmp(value, "yes")==0) {

			fs_options.ssh.flags |= _OPTIONS_SSH_FLAG_SUPPORT_CERTIFICATES;

		    } else if (strcmp(value, "0")==0 || strcmp(value, "no")==0) {

			if (fs_options.ssh.flags & _OPTIONS_SSH_FLAG_SUPPORT_CERTIFICATES) fs_options.ssh.flags -= _OPTIONS_SSH_FLAG_SUPPORT_CERTIFICATES;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "ssh.extensions")==0) {

		/* if extensions are defined in config that overrides the default
		    note that these extensions are only used when ext-info is enabled */

		if (len>0) {

		    fs_options.ssh.extensions=0;
		    parse_options_commalist(value, len, parse_ssh_extensions_cb, &fs_options.ssh);

		}

	    } else if (strcmp(option, "ssh.crypto_cipher")==0) {

		if ( len>0 ) {
		    unsigned int keep=fs_options.ssh.cipher;

		    /* cipher: allow none */

		    fs_options.ssh.cipher=0;
		    parse_options_commalist(value, len, parse_ssh_ciphers_cb, &fs_options.ssh);
		    if (fs_options.ssh.cipher==0) fs_options.ssh.cipher |= (keep==0) ? (1 << ((_OPTIONS_SSH_CIPHER_NONE - 1))) : keep;

		}

	    } else if (strcmp(option, "ssh.crypto_hmac")==0) {

		if ( len>0 ) {
		    unsigned int keep=fs_options.ssh.hmac;

		    /* hmac: allow none */

		    fs_options.ssh.hmac=0;
		    parse_options_commalist(value, len, parse_ssh_hmac_cb, &fs_options.ssh);
		    if (fs_options.ssh.hmac==0) fs_options.ssh.hmac |= (keep==0) ? (1 << ((_OPTIONS_SSH_HMAC_NONE - 1))) : keep;

		}

	    } else if (strcmp(option, "ssh.crypto_pubkey")==0) {

		if ( len>0 ) {
		    unsigned int keep=fs_options.ssh.pubkey;

		    fs_options.ssh.pubkey=0;
		    parse_options_commalist(value, len, parse_ssh_pubkey_cb, &fs_options.ssh);
		    if (fs_options.ssh.pubkey==0) fs_options.ssh.pubkey |= (keep==0) ? ((1 << ((_OPTIONS_SSH_PUBKEY_RSA - 1))) | (1 << (_OPTIONS_SSH_PUBKEY_ED25519 - 1))) : keep;

		}

	    } else if (strcmp(option, "ssh.crypto_compress")==0 || strcmp(option, "ssh.crypto_compression")==0) {

		if ( len>0 ) {
		    unsigned int keep=fs_options.ssh.compression;

		    /* compression: allow none */

		    fs_options.ssh.compression=0;
		    parse_options_commalist(value, len, parse_ssh_compress_cb, &fs_options.ssh);
		    if (fs_options.ssh.compression==0) fs_options.ssh.compression |= (keep==0) ? (1 << ((_OPTIONS_SSH_COMPRESS_NONE - 1))) : keep;

		}

	    } else if (strcmp(option, "ssh.crypto_keyx")==0 || strcmp(option, "ssh.crypto_keyexchange")==0) {

		if ( len>0 ) {
		    unsigned int keep=fs_options.ssh.keyx;

		    fs_options.ssh.keyx=0;
		    parse_options_commalist(value, len, parse_ssh_keyx_cb, &fs_options.ssh);
		    if (fs_options.ssh.keyx==0) fs_options.ssh.keyx |= (keep==0) ? ((1 << ((_OPTIONS_SSH_KEYX_DH - 1))) | (1 << ((_OPTIONS_SSH_KEYX_ECDH - 1)))) : keep;

		}

	    } else if ( strcmp(option, "ssh.timeout_init")==0 ) {

		if ( len>0 ) {

		    fs_options.ssh.timeout_init=atoi(value);

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if ( strcmp(option, "ssh.timeout_session")==0 ) {

		if ( len>0 ) {

		    fs_options.ssh.timeout_session=atoi(value);

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if ( strcmp(option, "ssh.timeout_exec")==0 ) {

		if ( len>0 ) {

		    fs_options.ssh.timeout_exec=atoi(value);

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if ( strcmp(option, "ssh.timeout_userauth")==0 ) {

		if ( len>0 ) {

		    fs_options.ssh.timeout_userauth=atoi(value);

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if ( strcmp(option, "sftp.usermapping.type")==0 ) {

		if ( len>0 ) {

		    if (strcmp(value, "file")==0) {

			fs_options.sftp.usermapping_type=_OPTIONS_SFTP_USERMAPPING_FILE;

		    } else if (strcmp(value, "none")==0) {

			fs_options.sftp.usermapping_type=_OPTIONS_SFTP_USERMAPPING_NONE;

		    } else if (strcmp(value, "map")==0) {

			fs_options.sftp.usermapping_type=_OPTIONS_SFTP_USERMAPPING_MAP;

		    } else {

			fprintf(stderr, "read_config: value %s for options %s not reckognized. Cannot continue.\n", value, option);
			result=-1;
			goto out;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "sftp.usermapping.user_unknown")==0 || strcmp(option, "sftp.usermapping.user_nobody")==0) {

		if ( len>0 ) {
		    char *tmp=strdup(value);

		    if (! tmp) {

			result=-1;
			fprintf(stderr, "read_config: option %s with value %s cannot be parsed (error %i). Cannot continue.\n", option, value, errno);
			goto out;

		    }

		    if (strcmp(option, "sftp.usermapping.user_unknown")==0) {

			fs_options.sftp.usermapping_user_unknown=tmp;

		    } else if (strcmp(option, "sftp.usermapping.user_nobody")==0) {

			fs_options.sftp.usermapping_user_nobody=tmp;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "sftp.network.name")==0) {

		if ( len>0 ) {

		    fs_options.sftp.network_name=strdup(value);

		    if (fs_options.sftp.network_name==NULL) {

			result=-1;
			fprintf(stderr, "read_config: option %s with value %s cannot be parsed (error %i). Cannot continue.\n", option, value, errno);
			goto out;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "sftp.network.show_domainname")==0) {

		if ( len>0 ) {

		    if (strcmp(value, "1")==0 || strcmp(value, "yes")==0) {

			fs_options.sftp.flags |= _OPTIONS_SFTP_FLAG_SHOW_DOMAINNAME;

		    } else if (strcmp(value, "0")==0 || strcmp(value, "no")==0) {

			fs_options.sftp.flags &= ~_OPTIONS_SFTP_FLAG_SHOW_DOMAINNAME;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "sftp.network.home_use_remotename")==0) {

		if ( len>0 ) {

		    if (strcmp(value, "1")==0 || strcmp(value, "yes")==0) {

			fs_options.sftp.flags |= _OPTIONS_SFTP_FLAG_HOME_USE_REMOTENAME;

		    } else if (strcmp(value, "0")==0 || strcmp(value, "no")==0) {

			fs_options.sftp.flags &= ~_OPTIONS_SFTP_FLAG_HOME_USE_REMOTENAME;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "sftp.network.hide_dot_files")==0) {

		if ( len>0 ) {

		    if (strcmp(value, "1")==0 || strcmp(value, "yes")==0) {

			fs_options.sftp.flags |= _OPTIONS_SFTP_FLAG_HIDE_DENTRIES;
			fs_options.sftp.hideflags |= _OPTIONS_SFTP_HIDE_FLAG_DOTFILE;

		    } else if (strcmp(value, "0")==0 || strcmp(value, "no")==0) {

			fs_options.sftp.hideflags &= ~_OPTIONS_SFTP_HIDE_FLAG_DOTFILE;
			if (fs_options.sftp.hideflags==0) fs_options.sftp.flags &= ~_OPTIONS_SFTP_FLAG_HIDE_DENTRIES;

		    }

		} else {

		    fprintf(stderr, "read_config: option %s requires an argument. Cannot continue.\n", option);
		    result=-1;
		    goto out;

		}

	    } else if (strcmp(option, "user.network.mount_template")==0) {

		if (len>0) {
		    struct pathinfo_s *pathinfo=&fs_options.user.network_mount_template;

		    pathinfo->path=strdup(value);

		    if (pathinfo->path) {

			pathinfo->len=strlen(value);
			pathinfo->flags=PATHINFO_FLAGS_ALLOCATED;

		    } else {

			fprintf(stderr, "read_config: option %s with value %s cannot be parsed (error %i). Cannot continue.\n", option, value, errno);
			result=-1;

		    }

		}

	    } else if (strcmp(option, "user.network.mount_group")==0) {
		struct group *grp;

		grp=getgrnam(value);
		if (grp) fs_options.user.network_mount_group=grp->gr_gid;

	    } else if (strcmp(option, "user.network.mount_group_policy")==0) {

		if (strcmp(value, "partof")==0) {

		    fs_options.user.flags |= _OPTIONS_USER_FLAG_NETWORK_GID_PARTOF;

		} else if (strcmp(value, "min")==0) {

		    fs_options.user.flags |= _OPTIONS_USER_FLAG_NETWORK_GID_MIN;

		}

	    }

	}


    }

    out:

    fclose(fp);
    if (line) free(line);

    return result;

}

int parse_arguments(int argc, char *argv[], unsigned int *error)
{
    static struct option long_options[] = {
	{"help", 		optional_argument, 		0, 0},
	{"version", 		optional_argument, 		0, 0},
	{"server", 		no_argument, 			0, 0},
	{"configfile", 		required_argument,		0, 0},
	{0,0,0,0}
	};
    int res, long_options_index=0, result=0;
    struct stat st;

    memset(&fs_options, 0, sizeof(struct fs_options_s));

    /* set defaults */

    init_pathinfo(&fs_options.configfile);
    init_pathinfo(&fs_options.socket);
    fs_options.flags=0;

    /* FUSE */

    convert_double_to_timespec(&fs_options.fuse.timeout_attr, _OPTIONS_FUSE_ATTR_TIMEOUT);
    convert_double_to_timespec(&fs_options.fuse.timeout_entry, _OPTIONS_FUSE_ENTRY_TIMEOUT);
    convert_double_to_timespec(&fs_options.fuse.timeout_negative, _OPTIONS_FUSE_NEGATIVE_TIMEOUT);
    fs_options.fuse.flags=0;

    /* NETWORK */

    fs_options.network.flags		=	0;
    fs_options.network.services		=	0;
    fs_options.network.discover		=	0;
    init_pathinfo(&fs_options.network.discover_static_file);
    fs_options.network.path_icon_network=	NULL;
    fs_options.network.path_icon_domain	=	NULL;
    fs_options.network.path_icon_server	=	NULL;
    fs_options.network.path_icon_share	=	NULL;
    fs_options.network.network_icon	=	_OPTIONS_NETWORK_ICON_OVERRULE;
    fs_options.network.domain_icon	=	_OPTIONS_NETWORK_ICON_OVERRULE;
    fs_options.network.server_icon	=	_OPTIONS_NETWORK_ICON_OVERRULE;
    fs_options.network.share_icon	=	_OPTIONS_NETWORK_ICON_OVERRULE;

    /* SSH */

    fs_options.ssh.flags		= 	0; // _OPTIONS_SSH_FLAG_SUPPORT_EXT_INFO | _OPTIONS_SSH_FLAG_SUPPORT_CERTIFICATES;

    /* default support all extensions mentioned in RFC 8308
	are there more ? */

    fs_options.ssh.extensions		=   	(1 << (_OPTIONS_SSH_EXTENSION_SERVER_SIG_ALGS - 1)) | (1 << (_OPTIONS_SSH_EXTENSION_DELAY_COMPRESSION - 1)) |
						(1 << (_OPTIONS_SSH_EXTENSION_NO_FLOW_CONTROL - 1)) | (1 << (_OPTIONS_SSH_EXTENSION_ELEVATION - 1));

    fs_options.ssh.cipher 		= 	_OPTIONS_SSH_CIPHER_AES256_CTR | _OPTIONS_SSH_CIPHER_CHACHA20_POLY1305_OPENSSH_COM;
    fs_options.ssh.compression		= 	_OPTIONS_SSH_COMPRESS_NONE;
    fs_options.ssh.pubkey		= 	_OPTIONS_SSH_PUBKEY_RSA | _OPTIONS_SSH_PUBKEY_ED25519;
    fs_options.ssh.certificate		= 	_OPTIONS_SSH_CERTIFICATE_RSA_CERT_V01_OPENSSH_COM | _OPTIONS_SSH_CERTIFICATE_ED25519_CERT_V01_OPENSSH_COM;
    fs_options.ssh.keyx			= 	_OPTIONS_SSH_KEYX_DH;
    fs_options.ssh.hmac			= 	_OPTIONS_SSH_HMAC_SHA256 | _OPTIONS_SSH_HMAC_SHA1;

    fs_options.ssh.timeout_init		= 	_OPTIONS_SSH_TIMEOUT_INIT_DEFAULT;
    fs_options.ssh.timeout_session	= 	_OPTIONS_SSH_TIMEOUT_SESSION_DEFAULT;
    fs_options.ssh.timeout_exec		= 	_OPTIONS_SSH_TIMEOUT_EXEC_DEFAULT;
    fs_options.ssh.timeout_userauth	= 	_OPTIONS_SSH_TIMEOUT_USERAUTH_DEFAULT;

    /* more backends... SSSD GPGME .... */

    fs_options.ssh.backend		= 	_OPTIONS_SSH_BACKEND_OPENSSH;
    fs_options.ssh.trustdb		= 	_OPTIONS_SSH_TRUSTDB_OPENSSH;

    fs_options.ssh.required_authmethods = 	_OPTIONS_SSH_SERVER_USERAUTH_REQUIRED_PUBLICKEY;
 
    /* SFTP */

    fs_options.sftp.usermapping_user_unknown=NULL;
    fs_options.sftp.usermapping_user_nobody=NULL;
    fs_options.sftp.usermapping_type=_OPTIONS_SFTP_USERMAPPING_DEFAULT;
    fs_options.sftp.usermapping_file=NULL;
    fs_options.sftp.flags = _OPTIONS_SFTP_FLAG_SHOW_NETWORKNAME | _OPTIONS_SFTP_FLAG_SHOW_DOMAINNAME | 
			    _OPTIONS_SFTP_FLAG_HOME_USE_REMOTENAME | _OPTIONS_SFTP_FLAG_SYMLINK_ALLOW_PREFIX;
    fs_options.sftp.hideflags = 0;
    fs_options.sftp.packet_maxsize=_OPTIONS_SFTP_PACKET_MAXSIZE;
    fs_options.sftp.network_name=NULL;

    /* NFS */

    fs_options.nfs.flags=_OPTIONS_NFS_FLAG_SHOW_DOMAINNAME;
    fs_options.nfs.packet_maxsize=_OPTIONS_NFS_PACKET_MAXSIZE;
    fs_options.nfs.network_name=NULL;

    /* USER */

    init_pathinfo(&fs_options.user.network_mount_template);
    fs_options.user.flags=0;
    fs_options.user.network_mount_group=0;

    while(1) {

	res = getopt_long(argc, argv, "", long_options, &long_options_index);
	if (res==-1) break;

	switch (res) {

	    case 0:

		/* a long option */

		if (strcmp(long_options[long_options_index].name, "help")==0) {

		    print_help(argv[0]);
		    result=1;
		    *error=0;
		    goto finish;

		} else if (strcmp(long_options[long_options_index].name, "version")==0) {

		    print_version(argv[0]);
		    result=1;
		    *error=0;
		    goto finish;

		} else if (strcmp(long_options[long_options_index].name, "server")==0) {

		    fs_options.flags |= _OPTIONS_MAIN_FLAG_SERVER;

		} else if (strcmp(long_options[long_options_index].name, "configfile")==0) {

		    if (optarg) {

			fs_options.configfile.path=realpath(optarg, NULL);

			if ( ! fs_options.configfile.path) {

			    result=-1;
			    *error=ENOMEM;
			    fprintf(stderr, "Error:(%i) option --configfile=%s cannot be parsed. Cannot continue.\n", errno, optarg);
			    goto out;

			} else {

			    fs_options.configfile.len=strlen(fs_options.configfile.path);
			    fs_options.configfile.flags=PATHINFO_FLAGS_ALLOCATED | PATHINFO_FLAGS_INUSE;

			}

		    } else {

			fprintf(stderr, "Error: option --configfile requires an argument. Cannot continue.\n");
			result=-1;
			*error=EINVAL;
			goto out;

		    }

		}

	    case '?':

		fprintf(stderr, "Error: option %s not reckognized.\n", optarg);
		result=-1;
		*error=EINVAL;
		goto finish;

	    default:

		fprintf(stdout,"Warning: getoption returned character code 0%o!\n", res);

	}

    }

    out:

    if (fs_options.configfile.path) {

	result=read_config(fs_options.configfile.path);

    } else {

	result=read_config(_OPTIONS_MAIN_CONFIGFILE);

    }

    if (result==-1) goto finish;

    if (fs_options.socket.path==NULL) {

	fs_options.socket.path=_OPTIONS_MAIN_SOCKET;
	fs_options.socket.len=strlen(fs_options.socket.path);

    }

    if (fs_options.network.services==0) {

	/* defaults */

	fs_options.network.services |= (_OPTIONS_NETWORK_ENABLE_SSH | _OPTIONS_NETWORK_ENABLE_SMB);

    }

    if (fs_options.network.discover & _OPTIONS_NETWORK_DISCOVER_METHOD_FILE) {

	if (fs_options.network.discover_static_file.path==NULL) {

	    /* take default */

	    fs_options.network.discover_static_file.path=_OPTIONS_NETWORK_DISCOVER_STATIC_FILE_DEFAULT;
	    fs_options.network.discover_static_file.len=strlen(fs_options.network.discover_static_file.path);

	}

    } else {

	if (fs_options.network.discover_static_file.path) {

	    /* not used */
	    free_path_pathinfo(&fs_options.network.discover_static_file);

	}

    }

    if (fs_options.sftp.flags==0) fs_options.sftp.flags|=_OPTIONS_SFTP_FLAG_HOME_USE_REMOTENAME;

    if (fs_options.user.network_mount_template.path==NULL) {

	fs_options.user.network_mount_template.path=_OPTIONS_USER_NETWORK_MOUNT_TEMPLATE_DEFAULT;
	fs_options.user.network_mount_template.len=strlen(fs_options.user.network_mount_template.path);

    }

    if (fs_options.user.network_mount_group==0) {
	struct group *grp;

	/* try some groups */

	grp=getgrnam("users");
	if (grp) {

	    fs_options.user.network_mount_group=grp->gr_gid;

	} else {

	    /* on some systems users group start at 1000 others at 100 */
	    fs_options.user.network_mount_group=_OPTIONS_USER_NETWORK_GID;

	}

    }

    if ((fs_options.user.flags & (_OPTIONS_USER_FLAG_NETWORK_GID_PARTOF | _OPTIONS_USER_FLAG_NETWORK_GID_MIN))==0) {

	    /* is there a way to determine the policy here?
		on some systems every user get's his own group
		on other they are part of the same group */

	fs_options.user.flags |= _OPTIONS_USER_FLAG_NETWORK_GID_DEFAULT;

    }

    finish:

    return result;

}

void free_options()
{
    free_path_pathinfo(&fs_options.configfile);
    free_path_pathinfo(&fs_options.socket);
    if (fs_options.sftp.network_name) free(fs_options.sftp.network_name);
    if (fs_options.sftp.usermapping_file) free(fs_options.sftp.usermapping_file);
    if (fs_options.sftp.usermapping_user_nobody) free(fs_options.sftp.usermapping_user_nobody);
    if (fs_options.sftp.usermapping_user_unknown) free(fs_options.sftp.usermapping_user_unknown);
    free_path_pathinfo(&fs_options.network.discover_static_file);
    free_path_pathinfo(&fs_options.user.network_mount_template);
}

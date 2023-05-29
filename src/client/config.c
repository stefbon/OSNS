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
#include "libosns-log.h"

#include "arguments.h"
#include "config.h"

static void parse_fuse_timeout_option(struct system_timespec_s *timeout, char *value)
{
    double tmp=strtod(value, NULL);
    convert_system_time_from_double(timeout, tmp);
}

static FILE *open_configfile(struct client_arguments_s *arg)
{
    FILE *fp=NULL;

#ifdef __linux__

    if (arg->configfile) fp=fopen(arg->configfile, "r");

#endif

    return fp;
}

static void set_flag_option(unsigned int *flags, unsigned int flag, char *value)
{

    if (strcmp(value, "1")==0 || strcmp(value, "yes")==0) {

	*flags |= flag;

    } else if (strcmp(value, "0")==0 || strcmp(value, "no")==0) {

	*flags &= ~flag;

    } else {

	logoutput_warning("set_flag_option: value %s not reckognized", value);

    }

}

int read_configfile(struct client_options_s *options, struct client_arguments_s *arg)
{
    FILE *fp;
    int result=0;
    char *line=NULL;
    char *sep;
    unsigned int len=0;
    size_t size=0;

    fp=open_configfile(arg);
    if (fp==NULL) return 0;

    while (result==0 && getline(&line, &size, fp)>0) {

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
	    if (strlen(option)==0 || option[0]== '#') continue;

	    len=strlen(value);
	    skip_heading_spaces(value, len);

	    logoutput("read_configfile: found %s:%s", option, value);

	    /* MAIN */

	    if (strcmp(option, "main.maxthreads")==0) {

		if (len>0) {

		    options->maxthreads=atoi(value);
		    if (options->maxthreads>OSNS_OPTIONS_MAIN_MAXTHREADS) options->maxthreads=OSNS_OPTIONS_MAIN_MAXTHREADS;

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}

	    /* FUSE */

	    } else if (strcmp(option, "fuse.timeout_attr")==0) {

		if (len>0) parse_fuse_timeout_option(&options->fuse.attr_timeout, value);

	    } else if (strcmp(option, "fuse.timeout_entry")==0) {

		if (len>0) parse_fuse_timeout_option(&options->fuse.entry_timeout, value);

	    } else if (strcmp(option, "fuse.timeout_negative")==0) {

		if (len>0) parse_fuse_timeout_option(&options->fuse.neg_timeout, value);

	    /* NETWORK */

	    } else if (strcmp(option, "network.show_icons")==0) {

		if ( len>0 ) {

		    set_flag_option(&options->network.flags, OSNS_OPTIONS_NETWORK_SHOW_ICON, value);

		} else {

		    logoutput_warning("read_configfile: option %s requires an argument. Cannot continue", option);
		    result=-1;

		}

	    } else if (strcmp(option, "network.show_networkname")==0) {

		if ( len>0 ) {

		    set_flag_option(&options->network.flags, OSNS_OPTIONS_NETWORK_SHOW_NETWORKNAME, value);

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}

	    } else if (strcmp(option, "network.show_domainname")==0) {

		if ( len>0 ) {

		    set_flag_option(&options->network.flags, OSNS_OPTIONS_NETWORK_SHOW_DOMAINNAME, value);

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}

	    } else if (strcmp(option, "network.hide_dotfiles")==0) {

		if ( len>0 ) {

		    set_flag_option(&options->network.flags, OSNS_OPTIONS_NETWORK_HIDE_DOTFILES, value);

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}

	    /* SSH */

	    } else if (strcmp(option, "ssh.trustdb_openssh")==0) {

		if ( len>0 ) {

		    set_flag_option(&options->ssh.flags, OSNS_OPTIONS_SSH_TRUSTDB_OPENSSH, value);

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}

	    /* SFTP */

	    } else if (strcmp(option, "sftp.network.maximum_packet_size")==0) {

		if (len>0) {

		    options->sftp.maxpacketsize=atoi(value);

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}
/*
	    } else if (strcmp(option, "sftp.network.home_use_remotename")==0) {

		if ( len>0 ) {

		    set_flag_option(&options->sftp.flags, OSNS_OPTIONS_SFTP_HOME_USE_REMOTENAME, value);

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		}

	    } else if (strcmp(option, "sftp.network.symlinks")==0) {

		if ( len>0 ) {

		    if (strcmp(value, "disable")==0) {

			options->sftp.flags |= OSNS_OPTIONS_SFTP_SYMLINKS_DISABLE;

		    } else if (strcmp(value, "allowcrossinterface")==0) {

			options->sftp.flags |= OSNS_OPTIONS_SFTP_ALLOW_CROSS_INTERFACE;

		    } else {

			options->sftp.flags |= OSNS_OPTIONS_SFTP_ALLOW_PREFIX;

		    }

		} else {

		    logoutput_warning("read_config: option %s requires an argument. Cannot continue.", option);
		    result=-1;

		} */

	    }

	}


    }

    out:

    fclose(fp);
    if (line) free(line);
    return result;

}

void set_default_options(struct client_options_s *options)
{
    struct system_timespec_s dummy=SYSTEM_TIME_INIT;

    /* MAIN */

    options->runpath=OSNS_DEFAULT_RUNPATH;
    options->etcpath=OSNS_DEFAULT_ETCPATH;
    options->group=OSNS_DEFAULT_UNIXGROUP;
    options->maxthreads=6;

    /* FUSE */

    if (system_time_test_earlier(&options->fuse.attr_timeout, &dummy)==0) set_system_time(&options->fuse.attr_timeout, 1, 0);
    if (system_time_test_earlier(&options->fuse.entry_timeout, &dummy)==0) set_system_time(&options->fuse.entry_timeout, 1, 0);
    if (system_time_test_earlier(&options->fuse.neg_timeout, &dummy)==0) set_system_time(&options->fuse.neg_timeout, 1, 0);
    options->fuse.maxread=OSNS_OPTIONS_FUSE_MAXREAD;

    /* NETWORK*/

    options->network.flags |= OSNS_OPTIONS_NETWORK_SHOW_ICON;
    options->network.flags |= OSNS_OPTIONS_NETWORK_SHOW_NETWORKNAME;
    options->network.flags |= OSNS_OPTIONS_NETWORK_SHOW_DOMAINNAME;
    options->network.flags |= OSNS_OPTIONS_NETWORK_HIDE_DOTFILES;

    options->network.name=OSNS_OPTIONS_NETWORK_NAME;
    options->network.flags |= OSNS_OPTIONS_NETWORK_DEFAULT_NETWORK_NAME;

    /* SSH */

    options->ssh.flags |= OSNS_OPTIONS_SSH_TRUSTDB_OPENSSH;

    /* SFTP */

    options->sftp.maxpacketsize=OSNS_OPTIONS_SFTP_PACKET_MAXSIZE;
//    options->sftp.flags |= OSNS_OPTIONS_SFTP_ALLOW_PREFIX;

}

void free_options(struct client_options_s *options)
{

    if (options->network.name && (options->network.flags & OSNS_OPTIONS_NETWORK_DEFAULT_NETWORK_NAME)==0) {

	free(options->network.name);
	options->network.name=NULL;

    }

}

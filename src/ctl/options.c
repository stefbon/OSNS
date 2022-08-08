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

#include <sys/stat.h>
#include <getopt.h>

#include "libosns-log.h"
#include "osnsctl.h"
#include "options.h"

static void print_help(const char *progname) {

    logoutput("General options:\n");
    logoutput("    --help                		print help\n");
    logoutput("    --version             		print version\n");
    logoutput("    --list (netcache|mountinfo)       	list records of network cache or mounted devices\n");
    logoutput("    --filter (%expression%)     	 	filter the list of records like (service=ssh) and (domain=example.org) (only for network)\n");
    logoutput("    --mount (network|devices)      	mount a FUSE filesystem for user\n");

    logoutput("\n");
    logoutput("\n");

}

static void print_version()
{
    logoutput("%i:%i\n", OSNS_CTL_VERSION, OSNS_CTL_MINOR_VERSION);
}

int parse_arguments(int argc, char *argv[], struct ctl_arguments_s *arguments)
{
    static struct option long_options[] = {
	{"help", 		optional_argument, 		0, 0},
	{"version", 		optional_argument, 		0, 0},
	{"list", 		required_argument, 		0, 0},
	{"filter", 		required_argument, 		0, 0},
	{"mount",		required_argument,		0, 0},
	{0,0,0,0}
    };
    int long_options_index=0;
    int result=0;

    while (result==0) {

	int tmp = getopt_long(argc, argv, "", long_options, &long_options_index);
	if (tmp==-1) break;

	switch (tmp) {

	    case 0:

		if (strcmp(long_options[long_options_index].name, "help")==0) {

		    print_help(argv[0]);
		    result=1;

		} else if (strcmp(long_options[long_options_index].name, "version")==0) {

		    print_version(argv[0]);
		    result=1;

		} else if (strcmp(long_options[long_options_index].name, "list")==0) {

		    if (arguments->type>0) {

			logoutput_error("parse_arguments: error: argument already specified.");
			result=-1;

		    } else {

			arguments->type=OSNS_COMMAND_TYPE_LIST;

			if (optarg) {

			    if (strcmp(optarg, "netcache")==0) {

				arguments->cmd.list.service=OSNS_LIST_TYPE_NETCACHE;
				arguments->init |= OSNS_INIT_FLAG_NETCACHE;

			    } else if (strcmp(optarg, "mountinfo")==0) {

				arguments->cmd.list.service=OSNS_LIST_TYPE_MOUNTINFO;
				arguments->init |= OSNS_INIT_FLAG_MOUNTINFO;

			    } else {

				logoutput_error("parse_arguments: error: list argument %s not reckognized.", optarg);
				result=-1;

			    }

			} else {

			    logoutput_error("parse_arguments: error: list argument requires argument.");

			}

		    }

		} else if (strcmp(long_options[long_options_index].name, "mount")==0) {

		    if (arguments->type>0) {

			logoutput_error("parse_arguments: error: argument already specified.");
			result=-1;

		    } else {

			arguments->type=OSNS_COMMAND_TYPE_MOUNT;

			if (optarg) {

			    if (strcmp(optarg, "network")==0) {

				arguments->cmd.mount.type=OSNS_MOUNT_TYPE_NETWORK;
				arguments->init |= OSNS_INIT_FLAG_MOUNT_NETWORK;

			    } else if (strcmp(optarg, "devices")==0) {

				arguments->cmd.mount.type=OSNS_MOUNT_TYPE_DEVICES;
				arguments->init |= OSNS_INIT_FLAG_MOUNT_DEVICES;

			    } else {

				logoutput_error("parse_arguments: error: list argument %s not reckognized.", optarg);
				result=-1;

			    }

			} else {

			    logoutput_error("parse_arguments: error: list argument requires argument.");

			}

		    }

		} else if (strcmp(long_options[long_options_index].name, "filter")==0) {

		    if (arguments->type==0) {

			logoutput_error("parse_arguments: error: no service to filter specified.");
			result=-1;

		    } else if ((arguments->type != OSNS_COMMAND_TYPE_LIST) || ((arguments->type == OSNS_COMMAND_TYPE_LIST) && arguments->cmd.list.filter.len>0)) {

			logoutput_error("parse_arguments: error: filter already specified or wrong command.");
			result=-1;

		    } else {

			arguments->cmd.list.filter.data=optarg;
			arguments->cmd.list.filter.len=strlen(optarg);

			if (arguments->cmd.list.service==OSNS_LIST_TYPE_NETCACHE) {

			    arguments->init |= OSNS_INIT_FLAG_FILTER_NETCACHE;

			} else if (arguments->cmd.list.service==OSNS_LIST_TYPE_MOUNTINFO) {

			    arguments->init |= OSNS_INIT_FLAG_FILTER_MOUNTINFO;

			}

		    }

		} else {

		    logoutput_error("parse_arguments: error: argument not supported/reckognized.");
		    result=-1;

		}

		break;

	    case '?':

		logoutput_error("Error: option %s not reckognized.\n", optarg);
		result=-1;
		break;

	    default:

		logoutput("Warning: getoption returned character code 0%o!\n", result);

	}

    }

    finish:
    return result;

}

void free_options()
{
}

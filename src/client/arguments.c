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

#include <getopt.h>

#include "libosns-log.h"
#include "osns_client.h"
#include "arguments.h"

static void print_help(const char *progname) {

    logoutput("General options:\n");
    logoutput("    --help                		print help\n");
    logoutput("    --version             		print version\n");
    logoutput("    --config=...             		path to configuration file\n");

    logoutput("\n");
    logoutput("\n");

}

static void print_version()
{
    logoutput("%i:%i\n", OSNS_CLIENT_VERSION, OSNS_CLIENT_MINOR);
}

int parse_arguments(int argc, char *argv[], struct client_arguments_s *arguments)
{
    static struct option long_options[] = {
	{"help", 		optional_argument, 		0, 0},
	{"version", 		optional_argument, 		0, 0},
	{"config", 		required_argument, 		0, 0},
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

		} else if (strcmp(long_options[long_options_index].name, "config")==0) {

		    if (arguments->configfile) {

			logoutput_error("parse_arguments: error: config file already specified.");
			result=-1;

		    }

		    if (optarg) {

			arguments->configfile=strdup(optarg);

			if (arguments->configfile==NULL) {

			    logoutput_error("parse_arguments: error: unable to allocate memory for argument configfile.");
			    result=-1;

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

    if (result==0) {

	if (arguments->configfile==NULL) {

	    arguments->configfile=OSNS_CLIENT_CONFIGFILE;
	    arguments->flags |= CLIENT_ARGUMENTS_FLAG_DEFAULT_CONFIGFILE;

	}

    }

    return result;

}

void free_arguments(struct client_arguments_s *arguments)
{

    if ((arguments->configfile) && (arguments->flags & CLIENT_ARGUMENTS_FLAG_DEFAULT_CONFIGFILE)==0) {

	free(arguments->configfile);
	arguments->configfile=NULL;

    }

}

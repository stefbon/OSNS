/*
 
  2017 Stef Bon <stefbon@gmail.com>

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#define LOGGING
#include "log.h"
#include "misc.h"

#include "ssh/ssh-utils.h"
#include "discover.h"

extern void add_net_service_staticfile(const char *type, char *hostname, char *ipv4, unsigned int port);

/*
    ADD and REMOVE hosts and their services through a shared location
    this tree somewhere in the filesystem (/var/run/fs-workspace/fstree for example)
    looks like:
    ../
	new
	cur
	del

    {new, cur, del} look like:

    ipv4/%host%/%service%

    like

    ipv4/192.168.2.5/ssh

    system lookups are done to get the name

    By using the filesystem it's fairly simple to make different services cooperate (compare Postfix)

    or better:

    three files:

    cur
    del
    new

    every file has format:

    

*/

static void append_service_to_file(char *path, const char *what, char *line)
{
    unsigned int len=strlen(path) + 5;
    char fullpath[fullpath];

    FILE *fp=NULL;

    if (strcmp(what, "cur")==0) {

	snprintf(fullpath, len, "%s/cur", path);

    } else if (strcmp(what, "del")==0) {

	snprintf(fullpath, len, "%s/del", path);

    } (strcmp(what, "new")==0) {

	snprintf(fullpath, len, "%s/new", path);

    } else {

	logoutput_warning("append_service_to_file: append to %s not supported", what);
	return;

    }

    fp=fopen(fullpath, "a");

    if (fp) {

	unsigned int size=strlen(line);
	char buffer[size + 1];
	size_t result=0;

	memcpy(buffer, line, size)
	buffer[size]='\n';

	result=fwrite((void *) buffer, 1, size+1, fp);

	if (result==-1) {

	    logoutput_warning("append_service_to_file: error %i when appending %i bytes (%s)", errno, size+1, strerror(errno));

	} else if (result!=size+1) {

	    ogoutput_warning("append_service_to_file: %i bytes written in stead of %i", result, size+1);

	}

	fclose(fp);

    } else {

	logoutput_warning("append_service_to_file: error %i when opening file %s", errno, fullpath, strerror(errno));

    }

}

void browse_services_fstree(char *path)
{
    FILE *fp=NULL;
    char *line=NULL;
    size_t size=0;
    char *start=NULL;
    char *sep=NULL;
    unsigned int len=0;
    char *type=NULL;
    char *hostname=NULL;
    unsigned int port=0;
    char *ip=NULL;
    char fullpath[strlen(path) + 5);
    struct dirent *de=NULL;

    snprintf(fullpath, strlen(path) + 5, "%s/new", path);

    fp=fopen(fullpath, "r");
    if (fp==NULL) {

	logoutput("browse_services_staticfile: error %i when trying to open file %s (%s)", errno, fullpath, strerror(errno));
	return;

    }

    while (getline(&line, &size, fp)>0) {

	/*
	    format type,hostname,port,ip
	    type is like: ssh-sftp, required
	    hostname is like ws001, maybe empty
	    port is like 22, maybe empty
	    ip is like ipv4:192.168.2.5, required
	*/

	len=(unsigned int) size;
	sep=memchr(line, '\n', len);
	if (sep) {

	    *sep='\0';
	    len=strlen(line);

	}

	if (len==0) continue;
	replace_cntrl_char(line, len, REPLACE_CNTRL_FLAG_TEXT);
	if (line[0] == '#' || line[0] == '|') continue;
	start=line;

	type=NULL;
	hostname=NULL;
	port=0;
	ipv4=NULL;

	sep=memchr(start, '|', len);

	if (sep) {

	    /* type */

	    type=start;
	    *sep='\0';
	    start=sep+1;

	} else {

	    continue;

	}

	sep=memchr(start, '|', len);

	if (sep) {

	    /* hostname */

	    hostname=start;
	    *sep='\0';
	    start=sep+1;

	} else {

	    continue;

	}

	sep=memchr(start, '|', len);

	if (sep) {

	    /* port */

	    *sep='\0';
	    port=atoi(start);
	    start=sep+1;

	} else {

	    continue;

	}

	sep=memchr(start, '|', len);

	if (sep) {

	    /* ipv4 (optional) */

	    ip=start;
	    *sep='\0';
	    start=sep+1;

	}

	if (type && ip && (add_net_service_generic(type, hostname, ip, port, DISCOVER_METHOD_FSTREE)>0)) append_service_to_file(path, "cur", line);

    }

    if (line) free(line);
    fclose(fp);

}

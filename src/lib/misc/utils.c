/*

  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include "utils.h"

void init_common_buffer(struct common_buffer_s *c_buffer)
{
    c_buffer->ptr=NULL;
    c_buffer->pos=NULL;
    c_buffer->len=0;
    c_buffer->size=0;
}

void free_common_buffer(struct common_buffer_s *c_buffer)
{

    if (c_buffer->ptr) {

	free(c_buffer->ptr);
	c_buffer->ptr=NULL;

    }

    init_common_buffer(c_buffer);

}

void unslash(char *p)
{
    char *q = p;
    char *pkeep = p;

    while ((*q++ = *p++) != 0) {

	if (q[-1] == '/') {

	    while (*p == '/') {

		p++;
	    }

	}
    }

    if (q > pkeep + 2 && q[-2] == '/') q[-2] = '\0';
}

void convert_to(char *string, int flags)
{
    char *p=string, *q=string;

    if (flags==0) return;

    for (p=string; *p != '\0'; ++p) {

	if ( flags & UTILS_CONVERT_SKIPSPACE ) {

	    if ( isspace(*p)) continue;

	}

	if ( flags & UTILS_CONVERT_TOLOWER ) {

	    *q=tolower(*p);

	} else {

	    *q=*p;

	}

	q++;

    }

    *q='\0';

}


/* a way to check two pids belong to the same process 
    to make this work process_id has to be the main thread of a process
    and thread_id is a process id of some thread of a process
    under linux then the directory
    /proc/<process_id>/task/<thread_id> 
    has to exist

    this does not work when both processes are not mainthreads
    20120426: looking for a better way to do this
*/

unsigned char belongtosameprocess(pid_t process_id, pid_t thread_id)
{
    char tmppath[40];
    unsigned char sameprocess=0;
    struct stat st;

    snprintf(tmppath, 40, "/proc/%i/task/%i", process_id, thread_id);

    if (lstat(tmppath, &st)==0) sameprocess=1;

    return sameprocess;
}

/* function to get the process id (PID) where the TID is given
   this is done by first looking /proc/tid exist
   if this is the case, then the tid is the process id
   if not, check any pid when walking back if this is
   the process id*/

pid_t getprocess_id(pid_t thread_id)
{
    pid_t process_id=thread_id;
    char path[40];
    DIR *dp=NULL;

    snprintf(path, 40, "/proc/%i/task", thread_id);

    dp=opendir(path);

    if (dp) {
	struct dirent *de=NULL;

	/* walk through directory /proc/threadid/task/ and get the lowest number: thats the process id */

	while((de=readdir(dp))) {

	    if (strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) continue;

	    thread_id=atoi(de->d_name);

	    if (thread_id>0 && thread_id<process_id) process_id=thread_id;

	}

	closedir(dp);

    }

    out:

    return process_id;

}

int custom_fork()
{
    int nullfd;
    int result=0;
    pid_t pid=fork();

    switch(pid) {

	case -1:

	    // logoutput("custom_fork: error %i:%s forking", errno, strerror(errno));
	    return -1;

	case 0:

	    break;

	default:

	    return (int) pid;

    }

    if (setsid() == -1) {

	// logoutput("custom_fork: error %i:%s setsid", errno, strerror(errno));
	return -1;
    }

    (void) chdir("/");

    nullfd = open("/dev/null", O_RDWR, 0);

    if (nullfd != -1) {

	(void) dup2(nullfd, 0);
	(void) dup2(nullfd, 1);
	(void) dup2(nullfd, 2);

	if (nullfd > 2) close(nullfd);

    }

    return 0;

}

uint32_t safe_atoi(char *b)
{
    char buffer[5];
    memcpy(buffer, b, 4);
    buffer[4]='\0';
    return (uint32_t) atoi(buffer);
}

uint64_t safe_atoii(char *b)
{
    char buffer[9];
    memcpy(buffer, b, 8);
    buffer[8]='\0';
    return (uint64_t) atol(buffer);
}

void strdup_target_path(char *target, char **p_path, unsigned int *error)
{
    char *path=strdup(target);

    *error=ENOMEM;

    if (path) {

	*p_path=path;
	*error=0;

    }

}

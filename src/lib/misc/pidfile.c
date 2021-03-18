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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <fcntl.h>

#include "log.h"
#include "utils.h"
#include "pathinfo.h"
#include "pidfile.h"

static unsigned int create_pid_path(char *path, char *name, char *user, pid_t pid, char *fullpath, unsigned int len)
{
    unsigned int pos=0;

    if (fullpath) {

	if (path) {

	    memcpy(&fullpath[pos], path, strlen(path));
	    pos+=strlen(path);
    	    fullpath[pos]='/';
	    pos++;

	}

	if (name) {

	    memcpy(&fullpath[pos], name, strlen(name));
	    pos+=strlen(name);
	    fullpath[pos]='-';
	    pos++;

	}

	if (user) {

	    memcpy(&fullpath[pos], user, strlen(user));
	    pos+=strlen(user);
	    fullpath[pos]='-';
	    pos++;

	}

	if (pid>0) pos+=snprintf(&fullpath[pos], len - pos, "%i.pid", pid);

    } else {

	pos+=((path) ? strlen(path) + 1 : 0) + 
		((name) ? strlen(name) + 1 : 0) +
		((user) ?  strlen(user) + 1 : 0) + 32;

    }

    return pos;

}

void create_pid_file(char *path, char *name, char *user, pid_t pid, char **p_keep)
{
    unsigned int len = create_pid_path(path, name, user, pid, NULL, 0);
    char fullpath[len];
    unsigned int pos=0;

    memset(fullpath, '\0', len);
    pos=create_pid_path(path, name, user, pid, fullpath, len);

    if (mknod(fullpath, S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)==0) {

	logoutput("create_pid_file: created pid file %s", fullpath);

	if (p_keep) *p_keep=strdup(fullpath);

    } else {

	logoutput("create_pid_file: error %i:%s creating pid file %s", errno, strerror(errno), fullpath);

    }

}

void remove_pid_file(char *path)
{
    unlink(path);
}

void remove_pid_fileat(int fd, char *name)
{
    unlinkat(fd, name, 0);
}

pid_t check_pid_file(char *path, char *name, char *user, int (* cb)(pid_t pid, void *ptr), unsigned int flags)
{
    unsigned int len1 = create_pid_path(NULL, name, user, 0, NULL, 0);
    char partname[len1];
    char *slash=NULL;
    pid_t pid=0;
    DIR *dp=NULL;
    struct dirent *de=NULL;
    unsigned int pos=0;
    int result=-1;

    memset(partname, '\0', len1);
    pos=create_pid_path(NULL, name, user, 0, partname, len1);

    dp=opendir(path);
    if (dp==NULL) return 0;
    de=readdir(dp);

    while (de) {

	if (strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) goto next;

	/* does this file also start with the desired name ? */

	if (strlen(de->d_name) > pos && memcmp(de->d_name, partname, pos)==0) {
	    char *sep=strrchr(de->d_name, '.');

	    if (sep && strcmp(sep, ".pid")==0) {

		*sep='\0';
		pid = (pid_t) atoi((char *)(de->d_name + pos));

		if (pid>0) {
		    unsigned int len2=create_pid_path(NULL, name, user, pid, NULL, 0);
		    char tmp[len2];

		    memset(tmp, '\0', len2);
		    pos=create_pid_path(NULL, name, user, pid, tmp, len2);

		    if (strcmp(de->d_name, tmp)==0) {

			result=(* cb)(pid, (void *)name);

			switch (result) {

			    case -2:
				break;
			    case -1:
			    case 0:

				if (flags & CHECK_PF_FLAG_REMOVE_IF_ORPHAN) {

				    remove_pid_fileat(dirfd(dp), de->d_name);
				    logoutput("check_pid_file: remove file %s", de->d_name);

				}

				break;

			    case 1:

				logoutput("check_pid_file: %s is already running (pid=%i)", de->d_name, pid);
				result=0;
				break;

			    default:

				logoutput("check_pid_file: result %i not reckognized", result);

			}

			/* pidfile found in path belongs to process pid and is of the right format */

			if (result==1) break;

		    }

		}

	    }

	}

	next:
	de=readdir(dp);

    }

    closedir(dp);
    return (result==1) ? pid : 0;

}

int create_fullpath(struct pathinfo_s *pathinfo)
{
    char path[pathinfo->len + 1];
    char *slash=NULL;

    memcpy(path, pathinfo->path, pathinfo->len);
    path[pathinfo->len] = '\0';
    unslash(path);

    /* create the parent path */

    slash=strchr(path, '/');

    while (slash) {

	*slash='\0';
	if (strlen(path)==0) goto next;

	if (mkdir(path, S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)==-1) {

	    if (errno != EEXIST) {

		logoutput("create_fullpath: error %i%s creating %s", errno, strerror(errno), path);
		return -1;

	    }

	}

	next:

	*slash='/';
	slash=strchr(slash+1, '/');

    }

    return 0;

}

int check_socket_path(struct pathinfo_s *pathinfo, unsigned int alreadyrunning)
{
    struct stat st;

    if (stat(pathinfo->path, &st)==0) {

	/* path to socket does exists */

	if (S_ISSOCK(st.st_mode)) {

	    if (alreadyrunning==0) {

		logoutput("check_socket_path: socket %s does already exist but no other process found, remove it", pathinfo->path);

		unlink(pathinfo->path);
		return 0;

	    } else {

		logoutput("check_socket_path: socket %s does already exist (running with pid %i), cannot continue", pathinfo->path, alreadyrunning);

	    }

	} else {

	    logoutput("check_socket_path: %s does already already exist (but is not a socket?!), cannot continue", pathinfo->path);

	}

	return -1;

    }

    return 0;

}

int check_pid_running(pid_t pid, char **p_cmdline)
{
    char procpath[64];
    int result=-1;

    if (pid==0) return -1;

    if (snprintf(procpath, 64, "/proc/%i/cmdline", (int) pid)>0) {
	struct stat st;

	if (stat(procpath, &st)==0) {

	    result=0;

	    if (p_cmdline) {
		int fd=open(procpath, O_RDONLY);

		if (fd>0) {
		    char buffer[st.st_size + 1];

		    memset(buffer, 0, st.st_size + 1);
		    ssize_t bytesread=read(fd, buffer, st.st_size + 1);

		    if (bytesread>0) *p_cmdline=strdup(buffer);
		    close(fd);

		}

	    }

	}

    }

    return result;
}


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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>

#include <pwd.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "misc.h"
#include "utils.h"

#undef LOGGING
#include "log.h"

/* function to get the "real" path from a template, which has has the 
   strings $HOME and $USER in it, which have to be replaced by the real value

   this can be programmed in a more generic way, but here only a small number fixed variables
   is to be looked for.. 

   return value: the converted string, which has to be freed later 
*/

char *get_path_from_template(char *template, struct passwd *pwd, char *buff, size_t len0)
{
    char *conversion=NULL;
    char *p1, *p2, *p1_keep;
    unsigned int len1, len2, len3, len4, len5;
    char path1[PATH_MAX];

    logoutput_notice("get_path_from_template: template %s", template);

    len1=strlen(template);

    len2=strlen("$HOME");
    len3=strlen(pwd->pw_dir);
    len4=strlen("$USER");
    len5=strlen(pwd->pw_name);

    p1=template;
    p2=path1;

    findnext:

    p1_keep=p1;
    p1=strchrnul(p1, '$');

    if ( *p1=='$' ) {

	if (p1 + len2 <= template + len1) {

	    if ( strncmp(p1, "$HOME", len2)==0 ) {

		if (p1>p1_keep) {

		    memcpy(p2, p1_keep, p1-p1_keep);
		    p2+=p1-p1_keep;

		}

		memcpy(p2, pwd->pw_dir, len3);
		p2+=len3;
		p1+=len2;

		goto findnext;

	    }

	}

	if (p1 + len4 <= template + len1) {

	    if ( strncmp(p1, "$USER", len4)==0 ) {

		if (p1>p1_keep) {

		    memcpy(p2, p1_keep, p1-p1_keep);
		    p2+=p1-p1_keep;

		}

		memcpy(p2, pwd->pw_name, len5);
		p2+=len5;
		p1+=len4;

		goto findnext;

	    }

	}

	/* when here: a $ is found, but it's not one of above */

	p1++;

    } else {

	/* $ not found, p1 points to end of string: maybe there is some left over */

	if ( p1>p1_keep ) {

	    memcpy(p2, p1_keep, p1-p1_keep);
	    p2+=p1-p1_keep;

	}

	/* terminator */

	*p2='\0';

    }

    if (p2!=path1) {

	/* size including the \0 terminator */

	len1=p2-path1+1;

	if ( buff ) {

	    /* store in the supplied buffer */

	    if ( len1<=len0 ) {

		conversion=buff;
		memcpy(conversion, path1, len1);

	    }

	} else {

	    /* create a new buffer */

	    conversion=malloc(len1);
	    if (conversion) memcpy(conversion, path1, len1);

	}

    }

    if (conversion) logoutput_notice("get_path_from_template: result %s", conversion);
    return conversion;

}

int create_directory(char *dir, mode_t mode, unsigned int *error)
{
    char path[strlen(dir) + 1];
    char *slash=NULL;
    unsigned int len=0;

    strcpy(path, dir);
    unslash(path);
    len=strlen(path);

    /* create the parent path */

    slash=strchrnul(path, '/');

    while (slash) {

	if (*slash=='/') *slash='\0';

	if (strlen(path)==0) goto next;

	if (mkdir(path, mode)==-1) {

	    if (errno != EEXIST) {

		logoutput("create_directory: error %i%s creating %s", errno, strerror(errno), path);
		*error=errno;
		return -1;

	    }

	}

	next:

	if ((int) (slash - path) >= len) {

	    break;

	} else {

	    *slash='/';
	    slash=strchrnul(slash+1, '/');

	}

    }

    return 0;

}

unsigned char ismounted(char *path)
{
    unsigned int len=strlen(path);
    char tmppath[strlen(path)+1];
    char *slash=NULL;
    unsigned char ismounted=0;
    struct stat st;

    memcpy(tmppath, path, len+1);
    slash=strrchr(tmppath, '/');

    if (slash && stat(tmppath, &st)==0) {
	dev_t dev=st.st_dev;

	*slash='\0';

	if (stat(tmppath, &st)==0) {

	    if (dev!=st.st_dev) ismounted=1;

	}

	*slash='/';

    }

    return ismounted;

}

unsigned char user_is_groupmember(char *username, struct group *grp)
{
    unsigned char found=0;
    char **member;

    member=grp->gr_mem;

    while(*member) {

	if (strcmp(username, *member)==0) {

	    found=1;
	    break;

	}

	member++;

    }

    return found;

}

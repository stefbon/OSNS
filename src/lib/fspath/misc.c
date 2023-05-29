/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

#include "fspath.h"
#include "copy.h"

unsigned int fs_path_get_length(struct fs_path_s *path)
{
    unsigned int len=0;

#ifdef __linux__

    if (path->buffer) len=path->len;

#endif

    return len;

}

char *fs_path_get_start(struct fs_path_s *path)
{
    char *pstart=NULL;

#ifdef __linux__

    if (path->buffer) pstart=(char *)(path->buffer + path->start);

#endif

    return pstart;

}

/* remove unneeded path elements
    this functions will remove elements like a double slash, a single dot and a double dot

    this function takes a path element, which is from the current position to the next slash (==/) or the end of the path when there is no slash anymore
    and check what to do with it. It can do three things:
    - append, it the default operation, when dealing with normal name
    - ignore, when dealing with a dot, or a double slash
    - up, when dealing with a double dot
*/

#define HANDLE_PATH_FLAG_FINISH					1

#define HANDLE_PATH_ACTION_APPEND				0
#define HANDLE_PATH_ACTION_IGNORE				1
#define HANDLE_PATH_ACTION_UP					2

struct path_element_s {
    unsigned char	flags;
    unsigned char	action;
    unsigned int 	len;
};

static void handle_path_element(char *pos, unsigned int len, struct path_element_s *pe)
{

    /* default: append ... override in special cases */

    pe->action=HANDLE_PATH_ACTION_APPEND;
    pe->len=len;

    if (len>=3) {

	if (len>=4 && strncmp(pos, "/../", 4)==0) {

	    pe->action=HANDLE_PATH_ACTION_UP;
	    pe->len=4;

	} else if (len==3 && (strncmp(pos, "/..", 3)==0 || strncmp(pos, "../", 3)==0)) {

	    pe->action=HANDLE_PATH_ACTION_UP;
	    pe->len=3;

	} else if (strncmp(pos, "/./", 3)==0 || strncmp(pos, "///", 3)==0) {

	    pe->action=HANDLE_PATH_ACTION_IGNORE;
	    pe->len=3;

	} else if (strncmp(pos, "//", 2)==0 || strncmp(pos, "/.", 2)==0) {

	    pe->action=HANDLE_PATH_ACTION_IGNORE;
	    pe->len=2;

	}

    } else if (len==2) {

	if (strncmp(pos, "/.", 2)==0 || strncmp(pos, "//", 2)==0) {

	    pe->action=HANDLE_PATH_ACTION_IGNORE;
	    pe->len=2;

	}

    } else if (len==1) {

	if (strncmp(pos, ".", 1)==0) {

	    pe->action=HANDLE_PATH_ACTION_IGNORE;
	    pe->len=1;

	}

    }

}

unsigned int fs_path_remove_unneeded_elements(struct fs_path_s *path)
{
    char *hlpr[2];
    char *pstart=(char *)(path->buffer + path->start);
    unsigned int plen=path->len;
    char *pos=pstart;
    int left=plen;
    unsigned int len=0;
    int result=-1;
    struct path_element_s pe;

    logoutput_debug("remove_unneeded_path_elements: path %.*s", left, pos);

    hlpr[0]=NULL;
    hlpr[1]=NULL;

    pe.flags=0;
    pe.action=0;
    pe.len=0;

    while (left>0 && (pe.flags & HANDLE_PATH_FLAG_FINISH)==0) {

	hlpr[1]=memchr(pos, '/', left);
	pe.len=0;
	pe.action=0;

	if (hlpr[1]==NULL) {

	    handle_path_element(pos, left, &pe);

	} else {

	    if (hlpr[1] > pstart) {

		handle_path_element(pos, (unsigned int)(hlpr[1] - pos + 1), &pe);

	    } else {

		pe.action=HANDLE_PATH_ACTION_APPEND;
		pe.len=1;

	    }

	}

	if (pe.action==HANDLE_PATH_ACTION_APPEND) {

	    pos         += pe.len;
	    left        -= pe.len;
	    len         += pe.len;
	    if (hlpr[1]) hlpr[0]=hlpr[1];

	} else if (pe.action==HANDLE_PATH_ACTION_IGNORE) {
	    char *new=pos + pe.len;

	    memmove(pos, new, left - pe.len);
	    pos         += pe.len;
	    left        -= pe.len;

	} else if (pe.action==HANDLE_PATH_ACTION_UP) {

	    if (hlpr[0]) {
		char *new=pos + pe.len;
		unsigned int tmp=(unsigned int)(pos - hlpr[0]);

		pos=hlpr[0] + 1;

		/* remove the previous path element -> len decreases by the length of this element */

		memmove(pos, new, left - pe.len);
		left-= pe.len;

	    } else {

		/* no slash[0] -> no obvious previous path element -> two possible reasons:
		    - must be the start of the path (which has no slash)
		    - or it's the first element */

		char *new=pos + pe.len;
		unsigned int tmp=(unsigned int)(pos - pstart);

		pos=pstart;
		memmove(pos, new, left - pe.len);
		left-= pe.len;

		path->back++;

	    }

	}

    }

    out:

    if (len < plen) {

	pos=(char *)(pstart + len);
	left=(unsigned int)(plen - len);
	memset(pos, 0, left);
	path->len=len;

    }

    return len;

}

#ifdef __linux__

int fs_path_get_target_unix_symlink(struct fs_path_s *path, struct fs_path_s *target)
{
    unsigned int len=fs_path_get_length(path);

    if (len>0) {
        char tmp[len + 1];
        char *buffer=NULL;
        unsigned int size=64;
        int bytesread=0;

        len=fs_path_copy(path, tmp, len);
        tmp[len]='\0';

        realloc:

        buffer=realloc(buffer, size);
        if (buffer==NULL) {

	    logoutput_warning("fs_path_get_target_unix_symlink: error %i allocating %i bytes (%s)", ENOMEM, size, strerror(ENOMEM));
	    return -ENOMEM;

        }

        memset(buffer, 0, size);
        bytesread=readlink(tmp, buffer, size);

        if (bytesread==-1) {
            unsigned int errcode=errno;

	    logoutput_warning("fs_path_get_target_unix_symlink: error %i reading path %s (%s)", errcode, tmp, strerror(errcode));
	    free(buffer);
	    return -errcode;

        } else if (bytesread >= size) {

	    /* possible the path is truncated ....*/
	    size+=64;
	    goto realloc;

        }

        target->start=0;
        target->buffer=buffer;
        target->size=size;
        target->len=(unsigned int) bytesread;
        target->flags |= FS_PATH_FLAG_BUFFER_ALLOC;
        return 0;

    }

    error:

    logoutput_warning("fs_path_get_target_unix_symlink: path not specified");
    return -EINVAL;

}

#else

int fs_path_get_target_unix_symlink(struct fs_path_s *path, struct fs_path_s *target)
{
    return -ENOSYS;
}

#endif

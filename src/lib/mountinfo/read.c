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

#include "libosns-basic-system-headers.h"

#include <sys/sysmacros.h>
#include <fcntl.h>

#include "libosns-log.h"
#include "libosns-list.h"
#include "libosns-eventloop.h"
#include "libosns-threads.h"

#include "monitor.h"
#include "list.h"
#include "mountentry.h"
#include "utils.h"

struct mountinfo_line_s {
    char 			*pos;
    int				left;
};

#ifdef __linux__

char *pseudofs[] = {"sysfs", "bdev", "proc", "cgroup", "cgroup2", "cpuset", "devtmpfs", "binfmt_misc", "configfs", "debugfs", "securityfs", "sockfs", "bpf", "pipefs", "ramfs", "hugetlbfs", "devpts", "mqueue", "pstore", "fusectl", "tracefs"};

static unsigned char test_fs_pseudofs(char *fs)
{
    unsigned char test=0;

    for (unsigned int i=0; i<(sizeof(pseudofs)/sizeof(pseudofs[0])); i++) {

	if (strcmp(pseudofs[i], fs)==0) {

	    test=1;
	    break;

	}

    }

    return test;

}

#else

static unsigned char test_fs_pseudofs(char *fs)
{
    return 0;
}

#endif

#define READ_MI_STRING_FLAG_EOL					1
#define READ_MI_STRING_FLAG_ZERO				2

static char *read_mountinfo_string(struct mountinfo_line_s *line, unsigned int flags)
{
    char *sep=NULL;
    char *str=NULL;

    /* skip heading spaces: if not doing so an empty string is the result  */

    while (memcmp(line->pos, " ", 1)==0) {

	if (line->left<=1) return NULL;
	line->pos++;
	line->left--;

    }

    sep=memchr(line->pos, ' ', line->left);

    if ((sep==NULL) && (flags & READ_MI_STRING_FLAG_EOL)) {

	sep=memchr(line->pos, '\n', line->left);

    }

    if ((sep==NULL) && (flags & READ_MI_STRING_FLAG_ZERO)) {

	sep=memchr(line->pos, '\0', line->left);

    }

    if (sep) {
	unsigned char tmpstore=*sep;

	*sep='\0';
	str=get_unescaped_string(line->pos);
	*sep=tmpstore;
	sep++;

	line->left-=(unsigned int)(sep - line->pos);
	line->pos=sep;

    }

    return str;
}

static char *find_mountinfo_char(struct mountinfo_line_s *line, int c)
{
    char *sep=memchr(line->pos, c, line->left);

    if (sep) {

	line->left-=(unsigned int)(sep - line->pos);
	line->pos=sep;

    }

    return sep;

}

#define FIND_MOUNTINFO_TOKEN_FLAG_SKIP					1

static char *find_mountinfo_token(struct mountinfo_line_s *line, char *token, unsigned int flags)
{
    char *start=line->pos;
    int left=line->left;
    unsigned int tmp=strlen(token);
    char *sep=NULL;

    while (left>0) {

	sep=memchr(start, token[0], left);

	if ((tmp + (unsigned int)(sep - start)) > left) {

	    sep=NULL;
	    break;

	}

	left-=(unsigned int)(sep - start);

	if (strncmp(sep, token, tmp)==0) {

	    if (flags & FIND_MOUNTINFO_TOKEN_FLAG_SKIP) {

		left -= tmp;
		sep += tmp;

	    }

	    /* found */
	    line->left=left;
	    line->pos=sep;
	    break;

	}

	start=sep+1;
	left--;

    }

    return sep;

}

static int read_mountid_majorminor(struct mountinfo_line_s *line, struct mountentry_s *me)
{
    char tmp[256];
    unsigned int len=0;
    unsigned int size=((line->left>255) ? 255 : line->left);

    /* logoutput_debug("read_mountid_majorminor: size %u", size); */

    memset(tmp, '\0', 256);
    memcpy(tmp, line->pos, size);

    /* with scanf and relatives there is no way to determine the position in the read buffer after a successfull read
	therefore I use the snprintf to reproduce the exact same first part of the buffer
	and snprintf gives a size == position in read buffer */

    if (sscanf(tmp, "%u %u %u:%u", &me->mountid, &me->parentid, &me->major, &me->minor) != 4) {

        logoutput_error("read_mountid_majorminor: error %u sscanf (%s)", errno, tmp);
	return -1;

    }

    len=snprintf(tmp, 256, "%u %u %u:%u", me->mountid, me->parentid, me->major, me->minor);

    if (line->left > len) {

	line->pos += len;
	line->left -= len;
	return 0;

    }

    return -1;

}

/* read one line from mountinfo */

static struct mountentry_s *read_mountinfo_line(struct mount_monitor_s *monitor, unsigned int size, unsigned int flags)
{
    struct mountentry_s *me=NULL;
    struct mountinfo_line_s line;
    struct mountentry_s entry;

    memset(&line, 0, sizeof(struct mountinfo_line_s));
    memset(&entry, 0, sizeof(struct mountentry_s));

    line.pos=monitor->buffer;
    line.left=size;

    if (read_mountid_majorminor(&line, &entry)==-1) {

	logoutput_error("read_mountinfo_line: unable to read mountid/parentid/major/minor");
	goto errorfree;

    }

    /* root */

    if (find_mountinfo_char(&line, '/')) {

	entry.rootpath=read_mountinfo_string(&line, 0);
	if (entry.rootpath==NULL) {

	    logoutput_debug("read_mountinfo_line: mountid %u empty rootpath", entry.mountid);
	    goto errorfree;

	}

    }

    /* mountpoint */

    if (find_mountinfo_char(&line, '/')) {

	entry.mountpoint=read_mountinfo_string(&line, 0);
	if (entry.mountpoint==NULL) {

	    logoutput_debug("read_mountinfo_line: mountid %u empty mountpoint", entry.mountid);
	    goto errorfree;

	}

    }

    /* skip mount options and optional fields,
	and start at the seperator - where filesystem, source and options are
	(20220317: maybe later the info about the mount namespace becomes relevant ...)
    */

    if (find_mountinfo_token(&line, " - ", FIND_MOUNTINFO_TOKEN_FLAG_SKIP)==NULL) {

	logoutput_debug("read_mountinfo_line: mountid %u token - not found", entry.mountid);
	goto errorfree;

    }

    /* filesystem */

    entry.fs=read_mountinfo_string(&line, 0);
    if (entry.fs==NULL) goto errorfree;

    if (flags & MOUNT_MONITOR_FLAG_IGNORE_PSEUDOFS) {

	if (test_fs_pseudofs(entry.fs)) goto errorfree;

    }

    /* source */

    entry.source=read_mountinfo_string(&line, 0);
    if (entry.source==NULL) {

	logoutput_debug("read_mountinfo_line: mountid %u empty source", entry.mountid);
	goto errorfree;

    }

    /* enough data to test to ignore or not */

    if ((* monitor->ignore) (entry.source, entry.fs, entry.mountpoint, monitor->data)==1) goto errorfree;

    /* options, it is the last data on this line,
	so a EOL or a ZERO maybe the terminator/seperator */

    entry.options=read_mountinfo_string(&line, (READ_MI_STRING_FLAG_EOL | READ_MI_STRING_FLAG_ZERO));

    /* get a new mountinfo */

    // logoutput_debug("read_mountinfo_line: found %s:%s at %s major:minor %i:%i", entry.source, entry.fs, entry.mountpoint, entry.major, entry.minor);

    me=create_mountentry(monitor, &entry);
    if (me==NULL) goto errorfree;
    return me;

    errorfree:
    free_mountentry_data(&entry);
    return NULL;
}

static int get_mountinfo_line(struct mount_monitor_s *monitor, FILE *fp)
{
    long cpos=ftell(fp);
    unsigned int len=0;

    readbuffer:

    memset(monitor->buffer, '\0', monitor->size);
    if (fgets(monitor->buffer, monitor->size, fp)==NULL) return -1;
    len=strlen(monitor->buffer);

    /* when buffer is (almost) full then probably too small: retry with a bigger buffer */

    if (len >= monitor->size - 1) {

	monitor->size += 512;
	monitor->buffer=realloc(monitor->buffer, monitor->size);
	if (monitor->buffer==NULL) return -1;
	logoutput_debug("get_mountinfo_line: buffer enlarged to %u bytes", monitor->size);
	fseek(fp, cpos, SEEK_SET);
	goto readbuffer;

    }

    return (int) len;

}

int get_mountinfo_list(struct mount_monitor_s *monitor, unsigned int flags)
{
    int error=0;
    FILE *fp=NULL;
    int size=0;

    if (monitor->buffer==NULL) {

	logoutput_debug("get_mountinfo_list: error: buffer empty");
	goto out;

    }

    logoutput_debug("get_mountinfo_list");

    fp=fopen_mountmonitor();
    if (fp==NULL) {

	logoutput_error("get_mountinfo_list: error opening mountmonitor");
	error=errno;
	goto out;

    }

    size=get_mountinfo_line(monitor, fp);

    while (size>0) {
	struct mountentry_s *me=NULL;

	me=read_mountinfo_line(monitor, size, flags);
	if (me) add_mountentry(monitor, me);
	size=get_mountinfo_line(monitor, fp);

    }

    fclose(fp);
    out:
    return (error>0) ? -error : 0;

}

/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-system.h"

#include "osns-protocol.h"

#define CHECK_PIDFILE_ALLOW_OTHERUSER				1
#define CHECK_PIDFILE_REMOVE_NONRUNNING				2
#define CHECK_PIDFILE_REMOVE_UNKNOWNEXE				4

#ifdef __linux__

static uid_t get_current_uid_running()
{
    return getuid();
}

static int get_pid_executable(unsigned int pid, struct fs_location_path_s *target)
{
    char path[64];
    int result=-EIO;

    if (snprintf(path, 64, "/proc/%i/exe", pid)>0) {

	result=get_target_unix_symlink(path, strlen(path), 0, target);

    }

    return result;

}

static int compare_pid_running(unsigned int pid, struct fs_location_path_s *target)
{
    struct fs_location_path_s compare=FS_LOCATION_PATH_INIT;
    int tmp=get_pid_executable(pid, &compare);
    int result=0;

    if (tmp==0) {

	result=1;
	if (compare_location_paths(&compare, target)==0) result=2;

    } else if (tmp==-ENOENT || tmp==-ENOTDIR) {

	result=-1;

    }

    clear_location_path(&compare);
    return result;

}

static unsigned int get_pidnumber_pidfile(char *name, unsigned int tmp, unsigned int len)
{
    unsigned int pid=0;

    logoutput_debug("get_pidnumber_pidfile: name %s tmp %u len %u", name, tmp, len);

    /* pid is stored from len + 1 to tmp - 5 */

    if (len + 1 < tmp - 5) {
	unsigned int width=(unsigned int)(tmp - 5 - len);
	char pidbuffer[width + 1];

	memset(pidbuffer, 0, width+1);
	memcpy(pidbuffer, &name[len+1], width);

	pid=strtol(pidbuffer, NULL, 10);

    }

    return pid;

}

int check_program_is_running(char *rundir, char *program, unsigned int flags)
{
    struct fs_location_path_s target=FS_LOCATION_PATH_INIT;
    unsigned int len=strlen(program);
    DIR *dp=NULL;
    struct dirent *de=NULL;
    int result=0;

    result=get_pid_executable(getpid(), &target);
    if (result<0) return -1;

    dp=opendir(rundir);
    if (dp==NULL) return 0;

    de=readdir(dp);
    while (de) {
	char *name=de->d_name;
	unsigned int tmp=strlen(name);
	unsigned int pid=0;

	/* the filename has be like: %program%-%pid%.pid
	    so at least len + 4 + 1 */

	if (tmp > len + 5) {

	    if (strcmp(&name[tmp-4], ".pid")==0 && memcmp(name, program, len)==0 && name[len]=='-') {

		pid=get_pidnumber_pidfile(name, tmp, len);
		if (pid==0) goto nextdirentryLabel;

		/* dealing with a pid belonging to an instance of this program ...
		    check its's running under the same user or not */

		if (flags & CHECK_PIDFILE_ALLOW_OTHERUSER) {
		    struct stat st;
		    uid_t uid=get_current_uid_running();

		    if (fstatat(dirfd(dp), name, &st, 0)==0 && (st.st_uid != uid)) pid=0;

		}

	    }

	}

	if (pid>0) {
	    int test=compare_pid_running(pid, &target);

	    if (test==-1) {

		/* pid is not running */

		if (flags & CHECK_PIDFILE_REMOVE_NONRUNNING) {

		    logoutput("check_program_is_running: removing %s (not running)", name);
		    unlinkat(dirfd(dp), name, 0);

		}

	    } else if (test==1) {

		/* pid is running but another exe */

		if (flags & CHECK_PIDFILE_REMOVE_UNKNOWNEXE) {

		    logoutput("check_program_is_running: removing %s (unknown exe)", name);
		    unlinkat(dirfd(dp), name, 0);

		}

	    } else if (test==2) {

		logoutput("check_program_is_running: already running with pid %u", pid);
		result=1;
		break;

	    }

	}

	nextdirentryLabel:
	de=readdir(dp);

    }

    closedir(dp);
    return result;

}

#else

int check_program_is_running(char *rundir, char *program, unsigned int flags)
{
    return 0;
}

#endif

int check_create_pid_file(char *rundir, char *progname, char **p_pidfile)
{
    char *slash=memrchr(progname, '/', strlen(progname));
    char *filename=(slash) ? slash + 1 : progname;
    int result=-1;

    if (check_program_is_running(rundir, filename, (CHECK_PIDFILE_ALLOW_OTHERUSER | CHECK_PIDFILE_REMOVE_NONRUNNING | CHECK_PIDFILE_REMOVE_UNKNOWNEXE))==1) {

	logoutput_warning("check_create_pid_file: %s already running", filename);

    } else {
	unsigned int len=strlen(rundir) + strlen(filename) + 32;
	char path[len];

	if (snprintf(path, len, "%s/%s-%u.pid", rundir, filename, getpid())>0) {

	    if (mknod(path, S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)==0) {

		logoutput("check_create_pid_file: created %s", path);
		if (p_pidfile) *p_pidfile=strdup(path);
		result=0;

	    } else {

		logoutput("check_create_pid_file: unable to create %s", path);

	    }

	}

    }

    return result;

}

void remove_pid_file(char *pidfile)
{
    unlink(pidfile);
}

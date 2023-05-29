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

static int get_pid_executable(unsigned int pid, struct fs_path_s *target)
{
    char procpath[64];
    int result=snprintf(procpath, 64, "/proc/%i/exe", pid);

    if (result>0) {
        struct fs_path_s tmp=FS_PATH_INIT;

        fs_path_assign_buffer(&tmp, procpath, (unsigned int) result);
	result=fs_path_get_target_unix_symlink(&tmp, target);

    }

    return result;

}

static int compare_pid_running(unsigned int pid, struct fs_path_s *target)
{
    struct fs_path_s compare=FS_PATH_INIT;
    int tmp=get_pid_executable(pid, &compare);
    int result=0;

    if (tmp==0) {

	result=1;
	if (fs_path_compare(&compare, 'p', target, NULL)==0) result=2;

    } else if (tmp==-ENOENT || tmp==-ENOTDIR) {

	result=-1;

    }

    fs_path_clear(&compare);
    return result;

}

static unsigned int get_id_pidfile(char *name, unsigned int len, const char *what)
{
    unsigned int id=0;
    char *sep=NULL;

    sep=memchr(name, '-', len);

    if (sep) {
	char buffer[len];

	memset(buffer, 0, len);

	if (strcmp(what, "uid")==0) {

	    memcpy(buffer, name, (unsigned int)(sep - name));

	} else if (strcmp(what, "pid")==0) {

	    memcpy(buffer, (sep + 1), (unsigned int)(name + len - sep - 1));

	}

	id=strtol(buffer, NULL, 10);

    }

    return id;

}

int check_program_is_running(char *rundir, char *program, unsigned int flags)
{
    struct fs_path_s target=FS_PATH_INIT;
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
	unsigned int pid_pidfile=0;
	unsigned int uid_pidfile=0;

	/* the filename has be like: %program%-%pid%.pid
	    so at least len + 4 + 1 */

	if (tmp > len + 5) {

	    if (strcmp(&name[tmp-4], ".pid")==0 && memcmp(name, program, len)==0 && name[len]=='-') {

		/* look at the name without the starting program name and the last bytes with ".pid" */

		uid_pidfile=get_id_pidfile(&name[len + 1], tmp - 5 - len, "uid");
		pid_pidfile=get_id_pidfile(&name[len + 1], tmp - 5 - len, "pid");
		if (pid_pidfile==0) goto nextdirentryLabel;

		logoutput_debug("check_program_is_running: found %s : pid %u uid %u", name, pid_pidfile, uid_pidfile);

		/* dealing with a pid belonging to an instance of this program ...
		    check its's running under the same user or not */

		if (flags & CHECK_PIDFILE_ALLOW_OTHERUSER) {
		    struct stat st;
		    uid_t uid=get_current_uid_running();

		    if (fstatat(dirfd(dp), name, &st, 0)==0) {

			if (st.st_uid != uid_pidfile) logoutput_warning("check_program_is_running: uid %u found is not same as owner of file (%u)", uid_pidfile, st.st_uid);
			if (st.st_uid != uid) pid_pidfile=0; /* belongs to another user */

		    }

		}

	    }

	}

	if (pid_pidfile>0) {
	    int test=compare_pid_running(pid_pidfile, &target);

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

		logoutput("check_program_is_running: already running with pid %u", pid_pidfile);
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
	unsigned int len=strlen(rundir) + strlen(filename) + 64;
	char path[len];

	if (snprintf(path, len, "%s/%s-%u-%u.pid", rundir, filename, getuid(), getpid())>0) {

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

unsigned int get_path_exe_pid(pid_t pid, struct fs_path_s *path)
{
    int tmp=get_pid_executable(pid, path);
    unsigned int len=0;

    if (tmp==0) len=path->len;
    return len;

}

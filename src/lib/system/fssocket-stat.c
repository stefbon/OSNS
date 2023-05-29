/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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

#include "stat.h"
#include "fssocket.h"

int fs_socket_fgetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    int fd=(* socket->get_unix_fd)(socket);

    if (mask==0) mask=SYSTEM_STAT_ALL;

#ifdef STATX_TYPE
    struct statx *stx=&stat->stx;

    if (statx(fd, "", AT_EMPTY_PATH, mask, stx)==-1) {

	logoutput_warning("fs_socket_fgetstat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask=stx->stx_mask;

#else
    struct stat *st=&sst->st;

    if (fstat(fd, st)==-1) {

	logoutput_warning("fs_socket_fgetstat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask = (mask & SYSTEM_STAT_BASIC_STATS);

#endif

    return 0;
}

int fs_socket_fsetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    int fd=(* socket->get_unix_fd)(socket);
    unsigned int error=0;

    stat->mask=0;

    if (mask & SYSTEM_STAT_SIZE) {
        off_t size=get_size_system_stat(stat);

	if (ftruncate(fd, size)==-1) {

	    logoutput_warning("fs_socket_fsetstat: error %i set size to %lu (%s)", errno, size, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("fs_socket_fsetstat: set size to %lu", size);
	    stat->mask |= SYSTEM_STAT_SIZE;
	    mask &= ~SYSTEM_STAT_SIZE;

	}

    }

    if (mask & (SYSTEM_STAT_UID | SYSTEM_STAT_GID)) {
	uid_t uid=(mask & SYSTEM_STAT_UID) ? get_uid_system_stat(stat) : (uid_t) -1;
	gid_t gid=(mask & SYSTEM_STAT_GID) ? get_gid_system_stat(stat) : (gid_t) -1;

	/* set the uid and gid
	    NOTE: using (uid_t) -1 and (gid_t) -1 in fchown will ignore this value
	    */

	if (fchown(fd, uid, gid)==-1) {

	    logoutput_warning("fs_socket_fsetstat: error %i set user and/or group (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("fs_socket_fsetstat: set uid %i and gid %i", uid, gid);
	    stat->mask |= ((mask & SYSTEM_STAT_UID) ? SYSTEM_STAT_UID : 0) | ((mask & SYSTEM_STAT_GID) ? SYSTEM_STAT_GID : 0);
	    mask &= ~(SYSTEM_STAT_UID | SYSTEM_STAT_GID);

	}

    }

    if (mask & SYSTEM_STAT_MODE) {
	mode_t mode=get_mode_system_stat(stat);

	if (fchmod(fd, mode)==-1) {

	    logoutput_warning("fs_socket_fsetstat: error %i set mode to %i (%s)", errno, mode, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("fs_socket_fsetstat: mode set to %i", mode);
	    stat->mask |= SYSTEM_STAT_MODE;
	    mask &= ~SYSTEM_STAT_MODE;

	}

    }

    if (mask & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME)) {
	struct timespec times[2];
	unsigned int todo=0;

	if (mask & SYSTEM_STAT_ATIME) {

	    times[0].tv_sec=get_atime_sec_system_stat(stat);
	    times[0].tv_nsec=get_atime_nsec_system_stat(stat);
	    todo|=SYSTEM_STAT_ATIME;

	} else {

	    times[0].tv_sec=0;
	    times[0].tv_nsec=UTIME_OMIT;

	}

	if (mask & SYSTEM_STAT_MTIME) {

	    times[1].tv_sec=get_mtime_sec_system_stat(stat);
	    times[1].tv_nsec=get_mtime_nsec_system_stat(stat);
	    todo|=SYSTEM_STAT_MTIME;

	} else {

	    times[1].tv_sec=0;
	    times[1].tv_nsec=UTIME_OMIT;

	}

	if (futimens(fd, times)==-1) {

	    logoutput_warning("fs_socket_fsetstat: error %i set atime and/or mtime (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	}

	stat->mask |= todo;
	mask &= ~todo;

    }

    if (mask & SYSTEM_STAT_CTIME) {

	logoutput_warning("fs_socket_fsetstat: change of ctime not supported/possible");
	mask &= ~SYSTEM_STAT_CTIME;

    }

    if (mask>0) {

	logoutput_warning("fs_socket_fsetstat: still actions (%i) requested but ignored (done %i)", mask, stat->mask);

    } else {

	logoutput("fs_socket_fsetstat: all actions done %i", stat->mask);

    }

    return 0;

    error:

    /* a rollback here ? then required a mask of operations done (successfully) and a backup of what is was before
	TODO?? */

    return -error;

}

int fs_socket_fgetstatat(struct fs_socket_s *socket, struct fs_path_s *path, unsigned int mask, struct system_stat_s *stat, unsigned int flags)
{
    int fd=(* socket->get_unix_fd)(socket);
    unsigned int size=fs_path_get_length(path);
    char name[size+1];

    if (size==0) {

        logoutput_error("fs_socket_fgetstatat: name/path is empty");
        return -1;

    }

    memset(name, 0, size+1);
    size=fs_path_copy(path, name, size);

    if (mask==0) mask=SYSTEM_STAT_ALL;
    flags |= ((stat->flags & SYSTEM_STAT_FLAG_FOLLOW_SYMLINK) ? 0 : AT_SYMLINK_NOFOLLOW);

#ifdef STATX_TYPE
    struct statx *stx=&stat->stx;

    if (statx(fd, name, flags, mask, stx)==-1) {

	logoutput_warning("fs_socket_fgetstatat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask=stx->stx_mask;

#else
    struct stat *st=&stat->st;

    if (fstatat(fd, name, st, flags)==-1) {

	logoutput_warning("fs_socket_fgetstatat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask = (mask & SYSTEM_STAT_BASIC_STATS);

#endif

    return 0;
}

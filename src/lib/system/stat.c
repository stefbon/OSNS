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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "log.h"
#include "misc.h"
#include "datatypes.h"

#include "open.h"
#include "path.h"
#include "location.h"
#include "stat.h"

#ifdef HAVE_STATX

int system_getstat(struct fs_location_path_s *path, unsigned int mask, struct system_stat_s *stat)
{
    struct statx *stx=&stat->stx;

    if (statx(0, path->ptr, AT_STATX_SYNC_AS_STAT, mask, stx)==-1) {

	logoutput_warning("system_getstat: error %i on path %s (%s)", errno, path->ptr, strerror(errno));
	return -errno;

    }

    stat->mask=stx->stx_mask;
    return 0;
}

int system_getlstat(struct fs_location_path_s *path, unsigned int mask, struct system_stat_s *stat)
{
    struct statx *stx=&stat->stx;

    if (statx(0, path->ptr, AT_STATX_DONT_SYNC | AT_SYMLINK_NOFOLLOW, mask, stx)==-1) {

	logoutput_warning("system_getlstat: error %i on path %s (%s)", errno, path->ptr, strerror(errno));
	return -errno;

    }

    stat->mask=stx->stx_mask;
    return 0;
}

int system_setstat(struct fs_location_path_s *path, unsigned int mask, struct system_stat_s *stat)
{
    struct statx *stx=&stat->stx;
    unsigned int error=0;

    stat->mask=0;

    if (mask & SYSTEM_STAT_SIZE) {

	if (truncate(path->ptr, stx->stx_size)==-1) {

	    logoutput_warning("system_setstat: error %i set size to %li (%s)", errno, (long unsigned int) stx->stx_size, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_setstat: size set to %li", stx->stx_size);
	    stat->mask |= SYSTEM_STAT_SIZE;
	    mask &= ~SYSTEM_STAT_SIZE;

	}

    }

    if (mask &(SYSTEM_STAT_UID | SYSTEM_STAT_GID)) {
	uid_t uid=(mask & SYSTEM_STAT_UID) ? stx->stx_uid : (uid_t) -1;
	gid_t gid=(mask & SYSTEM_STAT_GID) ? stx->stx_gid : (gid_t) -1;

	/* set the uid and gid
	    NOTE: using (uid_t) -1 and (gid_t) -1 in fchown will ignore this value
	    */

	if (fchownat(0, path->ptr, uid, gid, 0)==-1) {

	    logoutput("system_setstat: error %i set user and/or group (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    stat->mask |= ((mask & SYSTEM_STAT_UID) ? SYSTEM_STAT_UID : 0) | ((mask & SYSTEM_STAT_GID) ? SYSTEM_STAT_GID : 0);
	    mask &= ~(SYSTEM_STAT_UID | SYSTEM_STAT_GID);

	}

    }

    if (mask & SYSTEM_STAT_MODE) {
	mode_t mode=(stx->stx_mode & ~S_IFMT);

	if (fchmodat(0, path->ptr, mode, 0)==-1) {

	    logoutput("system_setstat: error %i set mode (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    stat->mask |= SYSTEM_STAT_MODE;
	    mask &= ~SYSTEM_STAT_MODE;

	}

    }

    if (mask & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME)) {
	struct timespec times[2];
	unsigned int todo=0;

	if (mask & SYSTEM_STAT_ATIME) {

	    times[0].tv_sec=stx->stx_atime.tv_sec;
	    times[0].tv_nsec=stx->stx_atime.tv_nsec;
	    todo |= SYSTEM_STAT_ATIME;

	} else {

	    times[0].tv_sec=0;
	    times[0].tv_nsec=UTIME_OMIT;

	}

	if (mask & SYSTEM_STAT_MTIME) {

	    times[1].tv_sec=stx->stx_mtime.tv_sec;
	    times[1].tv_nsec=stx->stx_mtime.tv_nsec;
	    todo |= SYSTEM_STAT_MTIME;

	} else {

	    times[1].tv_sec=0;
	    times[1].tv_nsec=UTIME_OMIT;

	}

	if (utimensat(0, path->ptr, times, 0)==-1) {

	    logoutput("system_setstat: error %i set atime and/or mtime (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	}

	stat->mask |= todo;
	mask &= ~todo;

    }

    if (mask & SYSTEM_STAT_CTIME) {

	logoutput("system_setstat: change of ctime not supported");
	mask &= ~ SYSTEM_STAT_CTIME;

    }

    if (mask>0) {

	logoutput_warning("setstat_system: still actions (%i) requested but ignored (done %i)", mask, stat->mask);

    } else {

	logoutput("setstat_system: all actions done %i", stat->mask);

    }

    return 0;

    error:

    /* a rollback here ? then required a mask of operations done (successfully) and a backup of what is was before
	TODO?? */

    return -error;

}

int system_fgetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    struct statx *stx=&stat->stx;
    int fd=get_unix_fd_fs_socket(socket);

    if (statx(fd, "", AT_EMPTY_PATH, mask, stx)==-1) {

	logoutput_warning("system_fgetstat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask=stx->stx_mask;
    return 0;
}

int system_fsetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    struct statx *stx=&stat->stx;
    int fd=get_unix_fd_fs_socket(socket);
    unsigned int error=0;

    stat->mask=0;

    if (mask & SYSTEM_STAT_SIZE) {

	if (ftruncate(fd, stx->stx_size)==-1) {

	    logoutput_warning("system_fsetstat: error %i set size to %li (%s)", errno, (long unsigned int) stx->stx_size, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_fsetstat: set size to %li", (long unsigned int) stx->stx_size);
	    stat->mask |= SYSTEM_STAT_SIZE;
	    mask &= ~SYSTEM_STAT_SIZE;

	}

    }

    if (mask & (SYSTEM_STAT_UID | SYSTEM_STAT_GID)) {
	uid_t uid=(mask & SYSTEM_STAT_UID) ? stx->stx_uid : (uid_t) -1;
	gid_t gid=(mask & SYSTEM_STAT_GID) ? stx->stx_gid : (gid_t) -1;

	/* set the uid and gid
	    NOTE: using (uid_t) -1 and (gid_t) -1 in fchown will ignore this value
	    */

	if (fchown(fd, uid, gid)==-1) {

	    logoutput_warning("system_fsetstat: error %i set user and/or group (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_fsetstat: set uid %i and gid %i", uid, gid);
	    stat->mask |= ((mask & SYSTEM_STAT_UID) ? SYSTEM_STAT_UID : 0) | ((mask & SYSTEM_STAT_GID) ? SYSTEM_STAT_GID : 0);
	    mask &= ~(SYSTEM_STAT_UID | SYSTEM_STAT_GID);

	}

    }

    if (mask & SYSTEM_STAT_MODE) {
	mode_t mode=(stx->stx_mode & ~S_IFMT);

	if (fchmod(fd, mode)==-1) {

	    logoutput_warning("system_fsetstat: error %i set mode to %i (%s)", errno, mode, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_fsetstat: mode set to %i", mode);
	    stat->mask |= SYSTEM_STAT_MODE;
	    mask &= ~SYSTEM_STAT_MODE;

	}

    }

    if (mask & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME)) {
	struct timespec times[2];
	unsigned int todo=0;

	if (mask & SYSTEM_STAT_ATIME) {

	    times[0].tv_sec=stx->stx_atime.tv_sec;
	    times[0].tv_nsec=stx->stx_atime.tv_nsec;
	    todo|=SYSTEM_STAT_ATIME;

	} else {

	    times[0].tv_sec=0;
	    times[0].tv_nsec=UTIME_OMIT;

	}

	if (mask & SYSTEM_STAT_MTIME) {

	    times[1].tv_sec=stx->stx_mtime.tv_sec;
	    times[1].tv_nsec=stx->stx_mtime.tv_nsec;
	    todo|=SYSTEM_STAT_MTIME;

	} else {

	    times[1].tv_sec=0;
	    times[1].tv_nsec=UTIME_OMIT;

	}

	if (futimens(fd, times)==-1) {

	    logoutput_warning("system_fsetstat: error %i set atime and/or mtime (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	}

	stat->mask |= todo;
	mask &= ~todo;

    }

    if (mask & SYSTEM_STAT_CTIME) {

	logoutput("system_fsetstat: change of ctime not supported/possible");
	mask &= ~SYSTEM_STAT_CTIME;

    }

    if (mask>0) {

	logoutput_warning("system_fsetstat: still actions (%i) requested but ignored (done %i)", mask, stat->mask);

    } else {

	logoutput("system_fsetstat: all actions done %i", stat->mask);

    }

    return 0;

    error:

    /* a rollback here ? then required a mask of operations done (successfully) and a backup of what is was before
	TODO?? */

    return -error;

}

int system_fgetstatat(struct fs_socket_s *socket, char *name, unsigned int mask, struct system_stat_s *stat)
{
    struct statx *stx=&stat->stx;
    int flags=(name) ? 0 : AT_EMPTY_PATH;
    int fd=get_unix_fd_fs_socket(socket);

    flags |= ((stat->flags & SYSTEM_STAT_FLAG_FOLLOW_SYMLINK) ? 0 : AT_SYMLINK_NOFOLLOW);

    if (statx(fd, name, flags, mask, stx)==-1) {

	logoutput_warning("system_fgetstatat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask=stx->stx_mask;
    return 0;
}

/* GET statx values */

uint64_t get_ino_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_ino;
}

uint16_t get_type_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_mode & S_IFMT;
}

uint16_t get_mode_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_mode & ~S_IFMT;
}

uint32_t get_nlink_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_nlink;
}

uint32_t get_uid_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_uid;
}

uint32_t get_gid_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_gid;
}

uint32_t get_size_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_size;

}

void get_atime_system_stat(struct system_stat_s *stat, struct system_timespec_s *atime)
{
    atime->st_sec=(int64_t) stat->stx.stx_atime.tv_sec;
    atime->st_nsec=(uint32_t) stat->stx.stx_atime.tv_nsec;
}

int64_t get_atime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_atime.tv_sec;
}

uint32_t get_atime_nsec_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_atime.tv_nsec;
}

void get_mtime_system_stat(struct system_stat_s *stat, struct system_timespec_s *mtime)
{
    mtime->st_sec=(int64_t) stat->stx.stx_mtime.tv_sec;
    mtime->st_nsec=(uint32_t) stat->stx.stx_mtime.tv_nsec;
}

int64_t get_mtime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_mtime.tv_sec;
}

uint32_t get_mtime_nsec_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_mtime.tv_nsec;
}

void get_ctime_system_stat(struct system_stat_s *stat, struct system_timespec_s *ctime)
{
    ctime->st_sec=(int64_t) stat->stx.stx_ctime.tv_sec;
    ctime->st_nsec=(uint32_t) stat->stx.stx_ctime.tv_nsec;
}

int64_t get_ctime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_ctime.tv_sec;
}

uint32_t get_ctime_nsec_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_ctime.tv_nsec;
}

void get_btime_system_stat(struct system_stat_s *stat, struct system_timespec_s *btime)
{
    btime->st_sec=(int64_t) stat->stx.stx_btime.tv_sec;
    btime->st_nsec=(uint32_t) stat->stx.stx_btime.tv_nsec;
}

int64_t get_btime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_btime.tv_sec;
}

uint32_t get_btime_nsec_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_btime.tv_nsec;
}

void get_dev_system_stat(struct system_stat_s *stat, struct system_dev_s *dev)
{
    dev->major=stat->stx.stx_dev_major;
    dev->minor=stat->stx.stx_dev_minor;
}

void get_rdev_system_stat(struct system_stat_s *stat, struct system_dev_s *dev)
{
    dev->major=stat->stx.stx_rdev_major;
    dev->minor=stat->stx.stx_rdev_minor;

}

uint32_t get_blocks_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_blocks;
}

uint32_t get_blksize_system_stat(struct system_stat_s *stat)
{
    return stat->stx.stx_blksize;
}

/* SET statx values */

void set_ino_system_stat(struct system_stat_s *stat, uint64_t ino)
{
    stat->stx.stx_ino=ino;
}

void set_type_system_stat(struct system_stat_s *stat, uint16_t type)
{
    uint16_t perm=(stat->stx.stx_mode & ~S_IFMT);

    stat->stx.stx_mode = (type & S_IFMT) | perm;
    stat->mask |= SYSTEM_STAT_TYPE;
}

void set_mode_system_stat(struct system_stat_s *stat, uint16_t mode)
{
    uint16_t type=(stat->stx.stx_mode & S_IFMT);

    stat->stx.stx_mode = type | (mode & ~S_IFMT);
    stat->mask |= SYSTEM_STAT_MODE;
}

void set_nlink_system_stat(struct system_stat_s *stat, uint32_t nlink)
{
    stat->stx.stx_nlink=nlink;
    stat->mask |= SYSTEM_STAT_NLINK;
}

void increase_nlink_system_stat(struct system_stat_s *stat, uint32_t count)
{
    stat->stx.stx_nlink+=count;
}

void decrease_nlink_system_stat(struct system_stat_s *stat, uint32_t count)
{

    if (count > stat->stx.stx_nlink) {

	stat->stx.stx_nlink=0;

    } else {

	stat->stx.stx_nlink-=count;

    }

}

void set_uid_system_stat(struct system_stat_s *stat, uint32_t uid)
{
    stat->stx.stx_uid=uid;
    stat->mask |= SYSTEM_STAT_UID;
}

void set_gid_system_stat(struct system_stat_s *stat, uint32_t gid)
{
    stat->stx.stx_gid=gid;
    stat->mask |= SYSTEM_STAT_GID;
}

void set_size_system_stat(struct system_stat_s *stat, uint64_t size)
{
    stat->stx.stx_size=size;
    stat->mask |= SYSTEM_STAT_SIZE;
}

/* with the statx a statx_timestamp is used, tv_sec is of type int64_t, tv_nsec of uint32_t */

void set_atime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->stx.stx_atime.tv_sec=(int64_t) time->st_sec;
    stat->stx.stx_atime.tv_nsec=(uint32_t) time->st_nsec;
    stat->mask |= SYSTEM_STAT_ATIME;
}

void set_atime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->stx.stx_atime.tv_sec=sec;
}

void set_atime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->stx.stx_atime.tv_nsec=nsec;
}

void set_mtime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->stx.stx_mtime.tv_sec=(int64_t) time->st_sec;
    stat->stx.stx_mtime.tv_nsec=(uint32_t) time->st_nsec;
    stat->mask |= SYSTEM_STAT_MTIME;
}

void set_mtime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->stx.stx_mtime.tv_sec=sec;
}

void set_mtime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->stx.stx_mtime.tv_nsec=nsec;
}

void set_ctime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->stx.stx_ctime.tv_sec=(int64_t) time->st_sec;
    stat->stx.stx_ctime.tv_nsec=(uint32_t) time->st_nsec;
    stat->mask |= SYSTEM_STAT_CTIME;
}

void set_ctime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->stx.stx_ctime.tv_sec=sec;
}

void set_ctime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->stx.stx_ctime.tv_nsec=nsec;
}

void set_btime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->stx.stx_btime.tv_sec=(int64_t) time->st_sec;
    stat->stx.stx_btime.tv_nsec=(uint32_t) time->st_nsec;
    stat->mask |= SYSTEM_STAT_BTIME;
}

void set_btime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->stx.stx_btime.tv_sec=sec;
}

void set_btime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->stx.stx_btime.tv_nsec=nsec;
}

void set_dev_system_stat(struct system_stat_s *stat, struct system_dev_s *dev)
{
    stat->stx.stx_dev_major=dev->major;
    stat->stx.stx_dev_minor=dev->minor;
}

void set_rdev_system_stat(struct system_stat_s *stat, struct system_dev_s *dev)
{
    stat->stx.stx_rdev_major=dev->major;
    stat->stx.stx_rdev_minor=dev->minor;
}

void copy_atime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->stx.stx_atime, &from->stx.stx_atime, sizeof(struct statx_timestamp));
}

void copy_mtime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->stx.stx_mtime, &from->stx.stx_mtime, sizeof(struct statx_timestamp));
}

void copy_ctime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->stx.stx_ctime, &from->stx.stx_ctime, sizeof(struct statx_timestamp));
}

void copy_btime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->stx.stx_btime, &from->stx.stx_btime, sizeof(struct statx_timestamp));
}

void set_blksize_system_stat(struct system_stat_s *stat, uint32_t blksize)
{
    stat->stx.stx_blksize=blksize;
}

#else

int system_getstat(struct fs_location_path_s *path, unsigned int mask, struct system_stat_s *stat)
{
    struct stat *st=&stat->st;

    if (stat(path->ptr, st)==-1) {

	logoutput_warning("system_getstat: error %i on path %s (%s)", errno, path->ptr, strerror(errno));
	return -errno;

    }

    stat->mask = (mask & SYSTEM_STAT_BASIC_STATS); /* stat return all attributes by default */
    return 0;
}

int system_getlstat(struct fs_location_path_s *path, unsigned int mask, struct system_stat_s *stat)
{
    struct stat *st=&stat->st;

    if (lstat(path->ptr, st)==-1) {

	logoutput_warning("system_getlstat: error %i on path %s (%s)", errno, path->ptr, strerror(errno));
	return -errno;

    }

    stat->mask = (mask & SYSTEM_STAT_BASIC_STATS); /* lstat return all attributes by default */
    return 0;
}

int system_setstat(struct fs_location_path_s *path, unsigned int mask, struct system_stat_s *stat)
{
    struct stat *st=&stat->st;
    unsigned int error=0;

    stat->mask=0;

    if (mask & SYSTEM_STAT_SIZE) {

	if (truncate(path->ptr, st->st_size)==-1) {

	    logoutput_warning("system_setstat: error %i set size to %li (%s)", errno, (long unsigned int) st->st_size, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_setstat: size set to %li", st->st_size);
	    stat->mask |= SYSTEM_STAT_SIZE;
	    mask &= ~SYSTEM_STAT_SIZE;

	}

    }

    if (mask &(SYSTEM_STAT_UID | SYSTEM_STAT_GID)) {
	uid_t uid=(mask & SYSTEM_STAT_UID) ? st->st_uid : (uid_t) -1;
	gid_t gid=(mask & SYSTEM_STAT_GID) ? st->st_gid : (gid_t) -1;

	/* set the uid and gid
	    NOTE: using (uid_t) -1 and (gid_t) -1 in fchown will ignore this value
	    */

	if (fchownat(0, path->ptr, uid, gid, 0)==-1) {

	    logoutput("system_setstat: error %i set user and/or group (%s)", errno, strerror(errno));
	    error=errno;
	    goto out;

	} else {

	    stat->mask |= ((mask & SYSTEM_STAT_UID) ? SYSTEM_STAT_UID : 0) | ((mask & SYSTEM_STAT_GID) ? SYSTEM_STAT_GID : 0);
	    mask &= ~(SYSTEM_STAT_UID | SYSTEM_STAT_GID);

	}

    }

    if (mask & SYSTEM_STAT_MODE) {
	mode_t mode=(st->st_mode & ~S_IFMT);

	if (fchmodat(0, path->ptr, mode, 0)==-1) {

	    logoutput("system_setstat: error %i set mode (%s)", errno, strerror(errno));
	    error=errno;
	    goto out;

	} else {

	    stat->mask |= SYSTEM_STAT_MODE;
	    mask &= ~SYSTEM_STAT_MODE;

	}

    }

    if (mask & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME)) {
	struct timespec times[2];
	unsigned int todo=0;

	if (mask & SYSTEM_STAT_ATIME) {

	    times[0].tv_sec=st->st_atime.sec;
	    times[0].tv_nsec=st->st_atime.nsec;
	    todo |= SYSTEM_STAT_ATIME;

	} else {

	    times[0].tv_sec=0;
	    times[0].tv_nsec=UTIME_OMIT;

	}

	if (mask & SYSTEM_STAT_MTIME]) {

	    times[1].tv_sec=st->st_mtime.sec;
	    times[1].tv_nsec=st->st_mtime.nsec;
	    todo |= SYSTEM_STAT_MTIME;

	} else {

	    times[1].tv_sec=0;
	    times[1].tv_nsec=UTIME_OMIT;

	}

	if (utimensat1(0, path->ptr, times, 0)==-1) {

	    logoutput("system_setstat: error %i set atime and/or mtime (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	}

	stat->mask |= todo;
	mask &= ~todo;

    }

    if (mask & SYSTEM_STAT_CTIME) {

	logoutput("system_setstat: change of ctime not supported");
	mask &= ~ SYSTEM_STAT_CTIME;

    }

    if (mask>0) {

	logoutput_warning("system_setstat: still actions (%i) requested but ignored (done %i)", mask, stat->mask);

    } else {

	logoutput("system_setstat: all actions done %i", stat->mask);

    }

    return 0;

    error:

    /* a rollback here ? then required a mask of operations done (successfully) and a backup of what is was before
	TODO?? */

    return -error;

}

int system_fgetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    struct stat *st=&stat->st;
    int fd=get_unix_fd_fssocket(socket);

    if (fstat(fd, st)==-1) {

	logoutput_warning("system_fgetstat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask = (mask & SYSTEM_STAT_BASIC_STATS);
    return 0;
}

int system_fsetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat)
{
    struct stat *st=&stat->st;
    int fd=get_unix_fd_fssocket(socket);
    unsigned int error=0;

    stat->mask=0;

    if (mask & SYSTEM_STAT_SIZE) {

	if (ftruncate(fd, st->st_size)==-1) {

	    logoutput_warning("system_fsetstat: error %i set size to %li (%s)", errno, (long unsigned int) st->s_size, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_fsetstat: set size to %li", (long unsigned int) st->st_size);
	    stat->mask |= SYSTEM_STAT_SIZE;
	    mask &= ~SYSTEM_STAT_SIZE;

	}

    }

    if (mask & (SYSTEM_STAT_UID | SYSTEM_STAT_GID)) {
	uid_t uid=(mask & SYSTEM_STAT_UID) ? st->st_uid : (uid_t) -1;
	gid_t gid=(mask & SYSTEM_STAT_GID) ? st->st_gid : (gid_t) -1;

	/* set the uid and gid
	    NOTE: using (uid_t) -1 and (gid_t) -1 in fchown will ignore this value
	    */

	if (fchown(fd, uid, gid)==-1) {

	    logoutput_warning("system_fsetstat: error %i set user and/or group (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_fsetstat: set uid %i and gid %i", uid, gid);
	    stat->mask |= ((mask & SYSTEM_STAT_UID) ? SYSTEM_STAT_UID : 0) | ((mask & SYSTEM_STAT_GID) ? SYSTEM_STAT_GID : 0);
	    mask &= ~(SYSTEM_STAT_UID | SYSTEM_STAT_GID);

	}

    }

    if (mask & SYSTEM_STAT_MODE) {
	mode_t mode=(st->st_mode & ~S_IFMT);

	if (fchmod(fd, mode)==-1) {

	    logoutput_warning("system_fsetstat: error %i set mode to %i (%s)", errno, mode, strerror(errno));
	    error=errno;
	    goto error;

	} else {

	    logoutput("system_fsetstat: mode set to %i", mode);
	    stat->mask |= SYSTEM_STAT_MODE;
	    mask &= ~SYSTEM_STAT_MODE;

	}

    }

    if (mask & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME)) {
	struct timespec times[2];
	unsigned int todo=0;

	if (mask & SYSTEM_STAT_ATIME) {

	    times[0].tv_sec=st->st_atime.sec;
	    times[0].tv_nsec=st->st_atime.nsec;
	    todo |= SYSTEM_STAT_ATIME;

	} else {

	    times[0].tv_sec=0;
	    times[0].tv_nsec=UTIME_OMIT;

	}

	if (mask & SYSTEM_STAT_MTIME) {

	    times[1].tv_sec=st->st_mtime.sec;
	    times[1].tv_nsec=st->st_mtime.nsec;
	    todo |= SYSTEM_STAT_MTIME;

	} else {

	    times[1].tv_sec=0;
	    times[1].tv_nsec=UTIME_OMIT;

	}

	if (futimens(fd, times)==-1) {

	    logoutput_warning("system_fsetstat: error %i set atime and/or mtime (%s)", errno, strerror(errno));
	    error=errno;
	    goto error;

	}

	stat->mask |= todo;
	mask &= ~todo;

    }

    if (mask & SYSTEM_STAT_CTIME) {

	logoutput("system_fsetstat: change of ctime not supported/possible");
	mask &= ~SYSTEM_STAT_CTIME;

    }

    if (mask>0) {

	logoutput_warning("system_fsetstat: still actions (%i) requested but ignored (done %i)", mask, stat->mask);

    } else {

	logoutput("system_fsetstat: all actions done %i", stat->mask);

    }

    return 0;

    error:

    /* a rollback here ? then required a mask of operations done (successfully) and a backup of what is was before
	TODO?? */

    return -error;

}

int system_fgetstatat(struct fs_socket_s *socket, char *name, unsigned int mask, struct system_stat_s *stat)
{
    struct stat *st=&stat->st;
    int flags=(name) ? 0 : AT_EMPTY_PATH;
    int fd=get_unix_fd_fssocket(socket);

    flags |= ((stat->flags & SYSTEM_STAT_FLAG_FOLLOW_SYMLINK) ? 0 : AT_SYMLINK_NOFOLLOW);

    if (fstatat(fd, name, st, flags)==-1) {

	logoutput_warning("system_fgetstatat: error %i on fd %i (%s)", errno, fd, strerror(errno));
	return -errno;

    }

    stat->mask = (mask & SYSTEM_STAT_BASIC_STATS);
    return 0;
}

/* GET stat values */

uint64_t get_ino_system_stat(struct system_stat_s *stat)
{
    return (uint64_t) stat->st.st_ino;
}

uint16_t get_type_system_stat(struct system_stat_s *stat)
{
    return (uint16_t) stat->st.st_mode & S_IFMT;
}

uint16_t get_mode_system_stat(struct system_stat_s *stat)
{
    return (uint16_t) stat->st.st_mode & ~S_IFMT;
}

uint32_t get_nlink_system_stat(struct system_stat_s *stat)
{
    return (uint32_t) stat->st.st_nlink;
}

uint32_t get_uid_system_stat(struct system_stat_s *stat)
{
    return (uint32_t) stat->st.st_uid;
}

uint32_t get_gid_system_stat(struct system_stat_s *stat)
{
    return (uint32_t) stat->st.st_gid;
}

uint32_t get_size_system_stat(struct system_stat_s *stat)
{
    return (uint32_t) stat->st.st_size;
}

void get_atime_system_stat(struct system_stat_s *stat, struct system_timespec_s *atime)
{
    atime->st_sec=(int64_t) stat->st.st_atim.tv_sec;
    atime->st_nsec=(uint32_t) stat->st.st_atim.tv_nsec;
}

int64_t get_atime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_atime.tv_sec;
}

uint32_t get_atime_nsec_system_stat(struct system_stat_s *stat)
{
    return (uint32_t) stat->stx.stx_atime.tv_nsec;
}

void get_mtime_system_stat(struct system_stat_s *stat, struct system_timespec_s *mtime)
{
    mtime->st_sec=(int64_t) stat->st.st_mtim.tv_sec;
    mtime->st_nsec=(uint32_t) stat->st.st_mtim.tv_nsec;
}

int64_t get_mtime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_mtime.tv_sec;
}

uint32_t get_mtime_nsec_system_stat(struct system_stat_s *stat)
{

    return (uint32_t) stat->stx.stx_mtime.tv_nsec;
}

void get_ctime_system_stat(struct system_stat_s *stat, struct system_timespec_s *ctime)
{
    ctime->st_sec=(int64_t) stat->st.st_ctim.tv_sec;
    ctime->st_nsec=(uint32_t) stat->st.st_ctim.tv_nsec;
}

int64_t get_ctime_sec_system_stat(struct system_stat_s *stat)
{
    return (int64_t) stat->stx.stx_ctime.tv_sec;
}

uint32_t get_ctime_nsec_system_stat(struct system_stat_s *stat)
{
    return (uint32_t) stat->stx.stx_ctime.tv_nsec;
}

void get_btime_system_stat(struct system_stat_s *stat, struct system_timespec_s *btime)
{
    btime->st_sec=0;
    btime->st_nsec=0;
}

int64_t get_btime_sec_system_stat(struct system_stat_s *stat)
{
    return 0;
}

uint32_t get_btime_nsec_system_stat(struct system_stat_s *stat)
{
    return 0;
}

void get_dev_system_stat(struct system_stat_s *stat, struct system_dev_t *dev)
{
    dev->major=major(stat->st.st_dev);
    dev->minor=minor(stat->st.st_dev);
}

void get_rdev_system_stat(struct system_stat_s *stat, struct system_dev_t *dev)
{
    dev->major=major(stat->st.st_rdev);
    dev->minor=minor(stat->st.st_rdev);

}

uint32_t get_blocks_system_stat(struct system_stat_s *stat)
{
    return stat->st.st_blocks;
}

uint32_t get_blksize_system_stat(struct system_stat_s *stat)
{
    return stat->st.st_blksize;
}

/* SET stat values */

void set_ino_system_stat(struct system_stat_s *stat, uint64_t ino)
{
    stat->st.st_ino=ino;
}

void set_type_system_stat(struct system_stat_s *stat, uint16_t type)
{
    uint16_t perm=(stat->st.st_mode & ~S_IFMT);

    stat->st.st_mode = (type & S_IFMT) | perm;
    stat->mask |= SYSTEM_STAT_TYPE;
}

void set_mode_system_stat(struct system_stat_s *stat, uint16_t mode)
{
    uint16_t type=(stat->st.st_mode & S_IFMT);

    stat->st.st_mode = type | (mode & ~S_IFMT);
    stat->mask |= SYSTEM_STAT_MODE;
}

void set_uid_system_stat(struct system_stat_s *stat, uint32_t uid)
{
    stat->st.st_uid=uid;
    stat->mask |= SYSTEM_STAT_UID;
}

void set_gid_system_stat(struct system_stat_s *stat, uint32_t gid)
{
    stat->st.st_gid=gid;
    stat->mask |= SYSTEM_STAT_GID;
}

void set_size_system_stat(struct system_stat_s *stat, uint64_t size)
{
    stat->st.st_size=size;
    stat->mask |= SYSTEM_STAT_SIZE;
}

void set_nlink_system_stat(struct system_stat_s *stat, uint32_t nlink)
{
    stat->st.st_nlink=nlink;
    stat->mask |= SYSTEM_STAT_NLINK;
}

void increase_nlink_system_stat(struct system_stat_s *stat, int32_t count)
{
    stat->st.st_nlink+=count;
}

void decrease_nlink_system_stat(struct system_stat_s *stat, int32_t count)
{

    if (count > stat->st.st_nlink) {

	stat->st.st_nlink=0;

    } else {

	stat->st.st_nlink-=count;

    }

}

/* with the stat a timespec is used, tv_sec is of type time_t, tv_nsec of long */

void set_atime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->st.st_atim.tv_sec=(time_t) time->st_sec;
    stat->st.st_atim.tv_nsec=(long) time->st_nsec;
    stat->mask |= SYSTEM_STAT_ATIME;
}

void set_atime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->st.st_atim.tv_sec=(time_t) sec;
}

void set_atime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->st.st_atim.tv_nsec=(long) nsec;
}

void set_mtime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->st.st_btim.tv_sec=(time_t) time->st_sec;
    stat->st.st_btim.tv_nsec=(long) time->st_nsec;
    stat->mask |= SYSTEM_STAT_MTIME;
}

void set_mtime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->st.st_mtim.tv_sec=(time_t) sec;
}

void set_mtime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->st.st_mtim.tv_nsec=(long) nsec;
}

void set_ctime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->st.st_ctim.tv_sec=(time_t) time->st_sec;
    stat->st.st_ctim.tv_nsec=(long) time->st_nsec;
    stat->mask |= SYSTEM_STAT_CTIME;
}

void set_ctime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
    stat->st.st_ctim.tv_sec=(time_t) sec;
}

void set_ctime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
    stat->st.st_ctim.tv_nsec=(long) nsec;
}

void set_btime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time)
{
    stat->mask |= SYSTEM_STAT_BTIME;
}

void set_btime_sec_system_stat(struct system_stat_s *stat, int64_t sec)
{
}

void set_btime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec)
{
}

void set_dev_system_stat(struct system_stat_s *stat, struct system_dev_t *dev)
{
    stat->st.st_dev=makedev(dev->major, dev->minor);
}

void set_rdev_system_stat(struct system_stat_s *stat, struct system_dev_t *dev)
{
    stat->st.st_rdev=makedev(dev->major, dev->minor);
}

void copy_atime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->st.st_atim, &from->st.st_atim, sizeof(struct timespec));
}

void copy_mtime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->st.st_mtim, &from->st.st_mtim, sizeof(struct timespec));
}

void copy_ctime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    memcpy(&to->st.st_ctim, &from->st.st_ctim, sizeof(struct timespec));
}

void copy_btime_system_stat(struct system_stat_s *to, struct system_stat_s *from)
{
    /* create time not supported for stat */
}

void set_blksize_system_stat(struct system_stat_s *stat, uint32_t blksize)
{
    stat->st.st_blksize=blksize;
}

#endif

struct active_system_stat_s {
    unsigned int				code;
    unsigned char				shift;
    void					(* cb)(struct system_stat_s *stat, void *ptr, unsigned char ctr);
};

uint32_t calc_amount_blocks(uint64_t size, uint32_t blksize)
{
    uint32_t count=(size / blksize);
    count += ((size % blksize)==0 ? 0 : 1);

    return count;
}

void calc_blocks_system_stat(struct system_stat_s *stat)
{
    uint32_t blksize=get_blksize_system_stat(stat);

    if (blksize>0) {
	uint64_t size=get_size_system_stat(stat);

	stat->sst_blocks=calc_amount_blocks(size, blksize);

    }

}

uint32_t get_unique_system_dev(struct system_dev_s *dev)
{
    return makedev(dev->major, dev->minor);
}

int system_stat_test_ISDIR(struct system_stat_s *stat)
{

#ifdef __linux__

    return S_ISDIR(stat->sst_mode);

#else

    return 0;

#endif

}

int system_stat_test_ISLNK(struct system_stat_s *stat)
{

#ifdef __linux__

    return S_ISLNK(stat->sst_mode);

#else

    return 0;

#endif

}

int system_getstatvfs(struct fs_location_path_s *path, struct system_statvfs_s *s)
{
#ifdef __linux__
    char tmp[path->len + 1];

    memcpy(tmp, path->ptr, path->len);
    tmp[path->len]='\0';

    return statvfs(tmp, &s->stvfs);

#else

    return -1;

#endif

}


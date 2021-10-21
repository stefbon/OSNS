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
#ifndef LIB_SYSTEM_STAT_H
#define LIB_SYSTEM_STAT_H

#include "path.h"
#include "open.h"

#ifdef __linux__

#include <sys/stat.h>

#ifdef HAVE_STATX

#define SYSTEM_STAT_TYPE			STATX_TYPE
#define SYSTEM_STAT_MODE			STATX_MODE
#define SYSTEM_STAT_NLINK			STATX_NLINK
#define SYSTEM_STAT_UID				STATX_UID
#define SYSTEM_STAT_GID				STATX_GID
#define SYSTEM_STAT_ATIME			STATX_ATIME
#define SYSTEM_STAT_MTIME			STATX_MTIME
#define SYSTEM_STAT_CTIME			STATX_CTIME
#define SYSTEM_STAT_INO				STATX_INO
#define SYSTEM_STAT_SIZE			STATX_SIZE
#define SYSTEM_STAT_BLOCKS			STATX_BLOCKS

#define SYSTEM_STAT_BTIME			STATX_BTIME
#define SYSTEM_STAT_MNTID			STATX_MNT_ID

#define SYSTEM_STAT_BASIC_STATS			(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_NLINK | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_INO | SYSTEM_STAT_SIZE | SYSTEM_STAT_BLOCKS)
#define SYSTEM_STAT_ALL				(SYSTEM_STAT_BASIC_STATS | SYSTEM_STAT_BTIME | SYSTEM_STAT_MNTID)

struct system_stat_s {
	unsigned int	mask;
	struct statx 	stx;
};

#else

#define SYSTEM_STAT_TYPE			(1 << 0)
#define SYSTEM_STAT_MODE			(1 << 1)
#define SYSTEM_STAT_NLINK			(1 << 2)
#define SYSTEM_STAT_UID				(1 << 3)
#define SYSTEM_STAT_GID				(1 << 4)
#define SYSTEM_STAT_ATIME			(1 << 5)
#define SYSTEM_STAT_MTIME			(1 << 6)
#define SYSTEM_STAT_CTIME			(1 << 7)
#define SYSTEM_STAT_INO				(1 << 8)
#define SYSTEM_STAT_SIZE			(1 << 9)
#define SYSTEM_STAT_BLOCKS			(1 << 10)

#define SYSTEM_STAT_BTIME			(1 << 11)
#define SYSTEM_STAT_MNTID			(1 << 12)

#define SYSTEM_STAT_BASIC_STATS			(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_NLINK | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_INO | SYSTEM_STAT_SIZE | SYSTEM_STAT_BLOCKS | SYSTEM_STAT_MNTID)
#define SYSTEM_STAT_ALL				SYSTEM_STAT_BASIC_STATS

struct system_stat_s {
	unsigned int	mask;
	struct stat	st;
};

#endif

#endif /* __linux__ */

struct system_timespec_s {
    uint64_t			sec;
    uint32_t			nsec;
};

#define SYSTEM_TIME_INIT	{0, 0}

/* Prototypes */

int system_getstat(struct fs_location_path_s *p, unsigned int mask, struct system_stat_s *stat);
int system_getlstat(struct fs_location_path_s *p, unsigned int mask, struct system_stat_s *stat);
int system_setstat(struct fs_location_path_s *p, unsigned int mask, struct system_stat_s *stat);

int system_fgetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat);
int system_fsetstat(struct fs_socket_s *socket, unsigned int mask, struct system_stat_s *stat);

int system_fgetstatat(struct fs_socket_s *socket, char *name, unsigned int mask, struct system_stat_s *stat);

uint32_t get_uid_system_stat(struct system_stat_s *stat);
uint32_t get_gid_system_stat(struct system_stat_s *stat);
uint32_t get_size_system_stat(struct system_stat_s *stat);
uint16_t get_type_system_stat(struct system_stat_s *stat);
uint16_t get_mode_system_stat(struct system_stat_s *stat);

void get_atime_system_stat(struct system_stat_s *stat, struct system_timespec_s *atime);
int64_t get_atime_sec_system_stat(struct system_stat_s *stat);
uint32_t get_atime_nsec_system_stat(struct system_stat_s *stat);

void get_btime_system_stat(struct system_stat_s *stat, struct system_timespec_s *btime);
int64_t get_btime_sec_system_stat(struct system_stat_s *stat);
uint32_t get_btime_nsec_system_stat(struct system_stat_s *stat);

void get_ctime_system_stat(struct system_stat_s *stat, struct system_timespec_s *ctime);
int64_t get_ctime_sec_system_stat(struct system_stat_s *stat);
uint32_t get_ctime_nsec_system_stat(struct system_stat_s *stat);

void get_mtime_system_stat(struct system_stat_s *stat, struct system_timespec_s *mtime);
int64_t get_mtime_sec_system_stat(struct system_stat_s *stat);
uint32_t get_mtime_nsec_system_stat(struct system_stat_s *stat);

void get_devino_system_stat(struct system_stat_s *stat, struct fs_location_devino_s *devino);

void set_type_system_stat(struct system_stat_s *stat, uint16_t type);
void set_mode_system_stat(struct system_stat_s *stat, uint16_t mode);
void set_uid_system_stat(struct system_stat_s *stat, uint32_t uid);
void set_gid_system_stat(struct system_stat_s *stat, uint32_t gid);
void set_size_system_stat(struct system_stat_s *stat, uint64_t size);

void set_atime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time);
void set_atime_sec_system_stat(struct system_stat_s *stat, int64_t sec);
void set_atime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec);

void set_mtime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time);
void set_mtime_sec_system_stat(struct system_stat_s *stat, int64_t sec);
void set_mtime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec);

void set_ctime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time);
void set_ctime_sec_system_stat(struct system_stat_s *stat, int64_t sec);
void set_ctime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec);

void set_btime_system_stat(struct system_stat_s *stat, struct system_timespec_s *time);
void set_btime_sec_system_stat(struct system_stat_s *stat, int64_t sec);
void set_btime_nsec_system_stat(struct system_stat_s *stat, uint32_t nsec);


#endif

/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Stef Bon <stefbon@gmail.com>

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

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-eventloop.h"

#include "defaults.h"
#include "receive.h"
#include "init.h"

#define OSNS_FUSE_VERSION				7
#define OSNS_FUSE_MINOR_VERSION				35

struct fuse_flags_s {
    uint32_t						flag;
    unsigned char					supported;
    unsigned int					avail;
    const char						*name;
};

static struct fuse_flags_s osns_fuse_flags[]={

#ifdef FUSE_ASYNC_READ
    /* 20220418: always do reads async */
    { .flag=FUSE_ASYNC_READ, .supported=FUSE_FS_PROFILE_ALL, .avail=7007, .name="asynchronous reads"},
#endif

#ifdef FUSE_POSIX_LOCKS
    /* 20220418: ignore posix locks */
    { .flag=FUSE_POSIX_LOCKS, .supported=0, .avail=7007, .name="support for posix style locks"},
#endif

#ifdef FUSE_FILE_OPS
    /* 20220418: always support calls like fstat and fsetstat using handle */
    { .flag=FUSE_FILE_OPS, .supported=FUSE_FS_PROFILE_ALL, .avail=7009, .name="support for fgetstat/fsetstat"},
#endif

#ifdef FUSE_ATOMIC_O_TRUNC
    /* 20220418: let filesystem handle the O_TRUNC open flag */
    { .flag=FUSE_ATOMIC_O_TRUNC, .supported=FUSE_FS_PROFILE_ALL, .avail=7009, .name="fs handles open flag O_TRUNC"},
#endif

#ifdef FUSE_EXPORT_SUPPORT
    /* 20220418: let kernel handle lookups of . and .. */
    { .flag=FUSE_EXPORT_SUPPORT, .supported=0, .avail=7010, .name="fs handles lookup . and .."},
#endif

#ifdef FUSE_BIG_WRITES
    /* 20220418: always support big writes > 4kB */
    { .flag=FUSE_BIG_WRITES, .supported=FUSE_FS_PROFILE_ALL, .avail=7010, .name="support big writes (>4Kb)"},
#endif

#ifdef FUSE_DONT_MASK
    /* 20220418: let kernel do the setting of modes using umaks */
    { .flag=FUSE_DONT_MASK, .supported=0, .avail=7012, .name="do not let the kernel/VFS adjust mode using mask"},
#endif

#ifdef FUSE_SPLICE_WRITE
    /* 20220418: do nothing with splice */
    { .flag=FUSE_SPLICE_WRITE, .supported=0, .avail=7014, .name="support for splice write"},
#endif

#ifdef FUSE_SPLICE_MOVE
    /* see above */
    { .flag=FUSE_SPLICE_MOVE, .supported=0, .avail=7014, .name="support for splice move"},
#endif

#ifdef FUSE_SPLICE_READ
    /* see above */
    { .flag=FUSE_SPLICE_READ, .supported=0, .avail=7014, .name="support for splice read"},
#endif

#ifdef FUSE_FLOCK_LOCKS
    /* 20220418: always support FILE LOCKS */
    { .flag=FUSE_FLOCK_LOCKS, .supported=FUSE_FS_PROFILE_ALL, .avail=7017, .name="support for file locks"},
#endif

#ifdef FUSE_HAS_IOCTL_DIR
    /* 20220418: for now do nothing with ioctl */
    { .flag=FUSE_HAS_IOCTL_DIR, .supported=0, .avail=7018, .name="support for IOCTL's on directories"},
#endif

#ifdef FUSE_AUTO_INVAL_DATA
    /* 20220418: for now let the kernel do the invalidation ... this may change */
    { .flag=FUSE_AUTO_INVAL_DATA, .supported=FUSE_FS_PROFILE_ALL, .avail=7020, .name="support for automatical invalidate data"},
#endif

#ifdef FUSE_DO_READDIRPLUS
    /* 20220418: for now do nothing with readdirplus */
    { .flag=FUSE_DO_READDIRPLUS, .supported=0, .avail=7021, .name="support for readdirplus"},
#endif

#ifdef FUSE_READDIRPLUS_AUTO
    /* see above */
    { .flag=FUSE_READDIRPLUS_AUTO, .supported=0, .avail=7021, .name="do readdirplus automatically"},
#endif

#ifdef FUSE_ASYNC_DIO
    /* 20220418: no direct I/O */
    { .flag=FUSE_ASYNC_DIO, .supported=0, .avail=7022, .name="support for asynchronous direct io"},
#endif

#ifdef FUSE_WRITEBACK_CACHE
    /* 20220418: enable caching of reads and writes
	this needs extra research, is the AUTO_INVAL_DATA a just combination here? */
    { .flag=FUSE_WRITEBACK_CACHE, .supported=FUSE_FS_PROFILE_ALL, .avail=7023, .name="support for writes using writeback cache"},
#endif

#ifdef FUSE_NO_OPEN_SUPPORT
    /* 20220418: no support for "no open" reads */
    { .flag=FUSE_NO_OPEN_SUPPORT, .supported=0, .avail=7023, .name="support for no open/direct reading"},
#endif

#ifdef FUSE_PARALLEL_DIROPS
    /* 20220418: always allow readdirs and lookups async */
    { .flag=FUSE_PARALLEL_DIROPS, .supported=FUSE_FS_PROFILE_ALL, .avail=7023, .name="support for parallel readdirs and lookups"},
#endif

#ifdef FUSE_HANDLE_KILLPRIV
    /* 20220418: let the fuse fs set the S_ISUID and S_ISGID bits  */
    { .flag=FUSE_HANDLE_KILLPRIV, .supported=0, .avail=7026, .name="fs supports unsetting the bits S_ISUID and S_ISGID"},
#endif

#ifdef FUSE_POSIX_ACL
    /* 20220418: ignore posix ACL's */
    { .flag=FUSE_POSIX_ACL, .supported=0, .avail=7026, .name="support for Posix ACL's"},
#endif

#ifdef FUSE_ABORT_ERROR
    /* 20220418: always return ECONNABORTED after abort */
    { .flag=FUSE_ABORT_ERROR, .supported=FUSE_FS_PROFILE_ALL, .avail=7027, .name="support for returning error ECONNABORTED"},
#endif

#ifdef FUSE_CACHE_SYMLINKS
    /* 20220418: always cache symlinks */
    { .flag=FUSE_CACHE_SYMLINKS, .supported=FUSE_FS_PROFILE_ALL, .avail=7028, .name="support for caching of symlinks"},
#endif

#ifdef FUSE_NO_OPENDIR_SUPPORT
    /* 20220418: no support for "no opendir" readdirs */
    { .flag=FUSE_NO_OPENDIR_SUPPORT, .supported=0, .avail=7029, .name="support for no opendir/direct readdirs"},
#endif

#ifdef FUSE_EXPLICIT_INVAL_DATA
    /* 20220418: always allow userspace and kernel to invalidate cached pages */
    { .flag=FUSE_EXPLICIT_INVAL_DATA, .supported=0, .avail=7030, .name="support only invalidate data on explicit request"},
#endif

#ifdef FUSE_MAP_ALLIGNMENT
    /* 20220418: do nothing with mapping */
    { .flag=FUSE_MAP_ALLIGNMENT, .supported=0, .avail=7031, .name="support map allignment"},
#endif

#ifdef FUSE_SUBMOUNTS
    /* 20220418: do nothing with submounts */
    { .flag=FUSE_SUBMOUNTS, .supported=0, .avail=7032, .name="support of submounts"},
#endif

#ifdef FUSE_HANDLE_KILLPRIV_V2
    /* 20220418: let the kernel set the S_ISUID and S_ISGID bits  */
    { .flag=FUSE_HANDLE_KILLPRIV_V2, .supported=0, .avail=7033, .name="fs supports unsetting the bits S_ISUID and S_ISGID v2"},
#endif

#ifdef FUSE_SETXATTR_EXT
    /* 20220418: do not use the extended setxattr in struct (with extra flags for unset SGID bit when system.posix_acl_access is set) */
    { .flag=FUSE_SETXATTR_EXT, .supported=0, .avail=7033, .name="support for extended setxattr"},
#endif

#ifdef FUSE_INIT_EXT
    /* 20220418: do nothing with extended fuse_init_in request */
    { .flag=FUSE_INIT_EXT, .supported=0, .avail=7036, .name="support for second flags field"},
#endif

};

int fuse_fs_init(struct fuse_receive_s *r, struct fuse_in_header *inh, char *data, unsigned char supported)
{
    struct fuse_init_in *init=(struct fuse_init_in *) data;
    struct fuse_init_out out;
    unsigned int versionnumber=0;
    int result=0;

    memset(&out, 0, sizeof(struct fuse_init_out));
    out.major=OSNS_FUSE_VERSION;
    out.minor=OSNS_FUSE_MINOR_VERSION;
    out.flags=0;

    logoutput("INIT (thread %i)", (int) gettid());
    logoutput("fuse_fs_init: kernel fuse protocol %u:%u", init->major, init->minor);
    logoutput("fuse_fs_init: userspace fuse protocol %u:%u", out.major, out.minor);

    if (init->major<7) {

	logoutput("fuse_fs_init: unsupported kernel protocol version");
	(* r->error_VFS)(r, inh->unique, EPROTO);
	signal_set_flag(r->loop->signal, &r->flags, FUSE_RECEIVE_FLAG_ERROR);
	return -1;

    } else if (init->major>7) {

	(* r->reply_VFS)(r, inh->unique, (char *) &out, sizeof(struct fuse_init_out));
	return 0;

    }

    /* out.major/out.minor is the version to be used */

    out.major=init->major;
    if (init->minor < out.minor) {

	logoutput_debug("fuse_fs_init: using kernel minor %u in stead of %u", init->minor, out.minor);
	out.minor=init->minor;

    }

    /* construct a version number to compare versions for available flags/options */

    versionnumber=(1000 * out.major) + out.minor;

    for (unsigned int i=0; i<((sizeof(osns_fuse_flags))/(sizeof(osns_fuse_flags[0]))); i++) {

	if ((init->flags & osns_fuse_flags[i].flag) && (osns_fuse_flags[i].avail <= versionnumber) && (supported & osns_fuse_flags[i].supported)) {

	    out.flags |= osns_fuse_flags[i].flag;
	    logoutput_debug("fuse_fs_init: %s", osns_fuse_flags[i].name);

	}

    }

    out.max_readahead = init->max_readahead;
    out.max_write = 4096; /* 4K */
    out.max_background=(1 << 16) - 1;
    out.congestion_threshold=(3 * out.max_background) / 4;

    logoutput("fuse_fs_init: using fuse protocol %u:%u", out.major, out.minor);
    logoutput("fuse_fs_init: max readahead %u", out.max_readahead);
    logoutput("fuse_fs_init: flags %u", out.flags);
    logoutput("fuse_fs_init: max write %u", out.max_write);

    result=(* r->reply_VFS)(r, inh->unique, (char *) &out, sizeof(struct fuse_init_out));
    signal_set_flag(r->signal, &r->flags, ((result>=0) ? FUSE_RECEIVE_FLAG_INIT : FUSE_RECEIVE_FLAG_ERROR));
    return result;

}

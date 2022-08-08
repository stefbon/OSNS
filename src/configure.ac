#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([osns_client], [1.0a1], [none])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([main.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h stddef.h stdlib.h string.h strings.h sys/stat.h sys/param.h sys/statvfs.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC

# AC_CANONICAL_HOST is needed to access the 'host_os' variable    
AC_CANONICAL_HOST

build_linux=no
build_windows=no
build_mac=no

# Detect the target system
case "${host_os}" in
    linux*)
        build_linux=yes
        ;;
    cygwin*|mingw*)
        build_windows=yes
        ;;
    darwin*)
        build_mac=yes
        ;;
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

# Pass the conditionals to automake
AM_CONDITIONAL([LINUX], [test "$build_linux" = "yes"])
AM_CONDITIONAL([WINDOWS], [test "$build_windows" = "yes"])
AM_CONDITIONAL([OSX], [test "$build_mac" = "yes"])

AC_CHECK_FUNCS([clock_gettime fdatasync ftruncate memmove memset mkdir realpath rmdir strchr strdup strerror strrchr strstr])
AC_CHECK_FUNCS([inotify_init], [AC_DEFINE([HAVE_INOTIFY], [1], [Check for inotify])])
AC_CHECK_FUNCS([statx],[AC_DEFINE([HAVE_STATX], [1], [Define if statx is available])], AC_MSG_NOTICE([Build without statx]))

AC_CHECK_HEADERS(stdio.h sys/param.h linux/fuse.h)
# AC_CHECK_HEADER([librsync.h],[AC_DEFINE([HAVE_LIBRSYNC], [1], [Define if librsync is available])], [AC_MSG_WARN([header librsync not found. Not building with support of fast network delta synchronization])])
# AC_CHECK_HEADER([magic.h], [AC_DEFINE([HAVE_LIBMAGIC], [1], [Define if libmagic is available])], [AC_MSG_WARN([required header magic.h not found])])
AC_CHECK_HEADER([pthread.h], [], [AC_MSG_ERROR([required header pthread.h not found])])
AC_CHECK_HEADER([security/pam_appl.h],[AC_DEFINE([HAVE_LIBPAM], [1], [Define if pam is available])], [AC_MSG_WARN([header pam_appl.h not found])])

# AC_SEARCH_LIBS([magic_open], [magic], [], [AC_MSG_ERROR([required library libmagic not found])])
AC_SEARCH_LIBS(pthread_create, pthread,, AC_MSG_ERROR([Please install pthreads library first.]))

PKG_CHECK_MODULES(GLIB, [ glib-2.0 ], [AC_DEFINE([HAVE_GLIB2], [1], [Define if Glib 2.0 is available])], AC_MSG_ERROR([Error: glib-2.0 not found.]))
PKG_CHECK_MODULES(GOBJECT, [ gobject-2.0 ], [AC_DEFINE([HAVE_GOBJECT2], [1], [Define if Gobject 2.0 is available])], AC_MSG_ERROR([Error: gobject-2.0 not found.]))
PKG_CHECK_MODULES(GIO, [ gio-2.0 ], [AC_DEFINE([HAVE_GIO2], [1], [Define if Gio 2.0 is available])], AC_MSG_ERROR([Error: gio-2.0 not found.]))

#PKG_CHECK_MODULES([LIBGCRYPT], [gcrypt], [AC_DEFINE([HAVE_LIBGCRYPT], [1], [Check for libgcrypt])], AC_MSG_ERROR([Please install libgcrypt.]))
#PKG_CHECK_MODULES(LIBSMB2, [ libsmb2 ], [AC_DEFINE([HAVE_LIBSMB2], [1], [Define if libsmbs is available])], AC_MSG_WARN([Warning: libsmb2 not found.]))

AM_PATH_LIBGCRYPT([1.7.0], [AC_DEFINE([HAVE_LIBGCRYPT], [1], [Check for libgcrypt])], AC_MSG_ERROR([Please install libgcrypt.]))

AC_ARG_WITH([osns-client], [AS_HELP_STRING([--with-osns-client], [Enable building OSNS osns client yes/no.])])

AS_IF([test "x$with_osns_client" = "xyes"], [
    AC_DEFINE([WITH_OSNS_CLIENT], [1], [Define to build OSNS client])
    AC_MSG_NOTICE([Build the osns client])
], [])

AC_ARG_WITH([osns-system], [AS_HELP_STRING([--with-osns-system], [Enable building OSNS osns system service yes/no.])])

AS_IF([test "x$with_osns_system" = "xyes"], [
    AC_DEFINE([WITH_OSNS_SYSTEM], [1], [Define to build OSNS system])
    AC_MSG_NOTICE([Build the osns system service.])

    PKG_CHECK_MODULES([AVAHI_CORE], [avahi-core], [AC_DEFINE([HAVE_AVAHI_CORE], [1], [Define Avahi core is available])], AC_MSG_ERROR([Error: avahi core not found.]))
    PKG_CHECK_MODULES([AVAHI_CLIENT], [avahi-client], [AC_DEFINE([HAVE_AVAHI_CLIENT], [1], [Define Avahi client is available])], AC_MSG_ERROR([Error: avahi client not found.]))
    PKG_CHECK_MODULES([AVAHI_GLIB], [avahi-glib], [AC_DEFINE([HAVE_AVAHI_GLIB], [1], [Define Avahi glib is available])], AC_MSG_ERROR([Error: avahi glib not found.]))

], [])

AC_ARG_WITH([osnsctl], [AS_HELP_STRING([--with-osnsctl], [Enable building OSNS osns ctl utility yes/no.])])

AS_IF([test "x$with_osnsctl" = "xyes"], [
    AC_DEFINE([WITH_OSNSCTL], [1], [Define to build OSNS ctl utility.])
    AC_MSG_NOTICE([Build the osns ctl utility.])
], [])

AC_ARG_WITH([sftp-subsystem], [AS_HELP_STRING([--with-sftp-subsystem], [Enable building OSNS sftp subsystem yes/no.])])

AS_IF([test "x$with_sftp_subsystem" = "xyes"], [
    AC_DEFINE([WITH_OSNS_SFTP_SUBSYSTEM], [1], [Define to build OSNS SFTP Substem])
    AC_MSG_NOTICE([Build the sftp subsystem])
], [])

AC_ARG_WITH([ssh-server], [AS_HELP_STRING([--with-ssh-server], [Enable building OSNS ssh server yes/no.])])

AS_IF([test "x$with_ssh_server" = "xyes"], [
    AC_DEFINE([WITH_OSNS_SFTP_SUBSYSTEM], [1], [Define to build OSNS SSH Server])
    AC_MSG_NOTICE([Build the sftp subsystem])
], [])

AS_IF([test LINUX], [

    AC_ARG_WITH([linux-epoll], [AS_HELP_STRING([--with-linux-epoll], [Enable building with epoll eventloop yes/no.])])

    AS_IF([ test "x$with_linux_epoll" = "xyes"], [

	AC_DEFINE([USE_LINUX_EPOLL], [1], [Define to use linux epoll])
	AC_MSG_NOTICE([Use the linux event poll subsystem.])

    ], [

	AS_IF([ test HAVE_GLIB2 ], [

	    AC_DEFINE([USE_GLIB_MAINLOOP], [1], [Define to use glib mainloop])
	    AC_MSG_NOTICE([Use the glib mainloop.])

	], [])

    ])
], [

    AS_IF([ test HAVE_GLIB2 ], [

	AC_DEFINE([USE_GLIB_MAINLOOP], [1], [Define to use glib mainloop])
	AC_MSG_NOTICE([Use the glib mainloop.])

    ], [])
])

PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [AC_DEFINE([HAVE_LIBSYSTEMD], [1], [Define libsystemd is available])], AC_MSG_ERROR([Error: libsystemd not found.]))

AM_CONDITIONAL([BUILD_OSNS_CLIENT], [ test "x$with_osns_client" = "xyes" ])
AM_CONDITIONAL([BUILD_OSNS_SYSTEM], [ test "x$with_osns_system" = "xyes" ])
AM_CONDITIONAL([BUILD_OSNSCTL], [ test "x$with_osnsctl" = "xyes" ])
AM_CONDITIONAL([BUILD_SFTP_SUBSYSTEM], [ test "x$with_sftp_subsystem" = "xyes" ])
AM_CONDITIONAL([BUILD_SSH_SERVER], [ test "x$with_ssh_server" = "xyes" ])

AM_CONDITIONAL([WITH_LINUX_EPOLL], [ test "x$with_linux_epoll" = "xyes"])

# AM_CONDITIONAL([LIBRSYNC_AVAIL], [ test HAVE_LIBRSYNC ])
# AM_CONDITIONAL([LIBMAGIC_AVAIL], [ test HAVE_LIBMAGIC ])

AM_CONDITIONAL([LIBPAM_AVAIL], [ test HAVE_LIBPAM ])
AM_CONDITIONAL([GLIB2_AVAIL], [ test HAVE_GLIB2 ])
AM_CONDITIONAL([GOBJECT2_AVAIL], [ test HAVE_GOBJECT2 ])
AM_CONDITIONAL([GIO2_AVAIL], [ test HAVE_GIO2 ])

# AM_CONDITIONAL([LIBSMB2_AVAIL], [ test HAVE_LIBSMB2 ])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

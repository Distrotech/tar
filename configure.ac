# Configure template for GNU tar.

# Copyright (C) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
# 2002, 2003, 2004 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

AC_INIT([GNU tar], [1.13.92a], [bug-tar@gnu.org])
AC_CONFIG_SRCDIR([src/tar.c])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h:config.hin])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([1.8 gnits dist-bzip2 dist-shar std-options])

gl_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_EXEEXT
AC_PROG_RANLIB
AC_SYS_LARGEFILE
AC_ISC_POSIX
AC_C_INLINE

AC_CHECK_HEADERS(fcntl.h linux/fd.h memory.h net/errno.h \
  sgtty.h string.h \
  sys/buf.h sys/device.h sys/gentape.h \
  sys/inet.h sys/io/trioctl.h \
  sys/mtio.h sys/tprintf.h sys/tape.h \
  unistd.h)

AC_HEADER_SYS_WAIT
AM_STDBOOL_H

if test $ac_cv_header_sys_mtio_h = yes; then
  AC_CACHE_CHECK(for remote tape header files, tar_cv_header_rmt,
    [AC_TRY_CPP([
#if HAVE_SGTTY_H
# include <sgtty.h>
#endif
#include <sys/socket.h>],
    tar_cv_header_rmt=yes, tar_cv_header_rmt=no)])
  test $tar_cv_header_rmt = yes && RMT='rmt$(EXEEXT)'
  AC_SUBST(RMT)
fi

AC_CACHE_CHECK(which ioctl field to test for reversed bytes,
  tar_cv_header_mtio_check_field,
  [AC_EGREP_HEADER(mt_model, sys/mtio.h,
  tar_cv_header_mtio_check_field=mt_model,
  tar_cv_header_mtio_check_field=mt_type)])
AC_DEFINE_UNQUOTED(MTIO_CHECK_FIELD, $tar_cv_header_mtio_check_field,
  [Define to mt_model (v.g., for DG/UX), else to mt_type.])

AC_HEADER_DIRENT
AC_HEADER_MAJOR
AC_HEADER_STAT
AC_HEADER_STDC
AC_STRUCT_ST_BLKSIZE
AC_STRUCT_ST_BLOCKS
AC_MSG_CHECKING([for st_fstype string in struct stat])
AC_CACHE_VAL(diff_cv_st_fstype_string,
  [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/stat.h>], [struct stat s; s.st_fstype[0] = 'x';],
   diff_cv_st_fstype_string=yes,
   diff_cv_st_fstype_string=no)])
AC_MSG_RESULT($diff_cv_st_fstype_string)
if test $diff_cv_st_fstype_string = yes; then
  AC_DEFINE(HAVE_ST_FSTYPE_STRING, 1,
    [Define if struct stat has a char st_fstype[] member.])
fi

AC_TYPE_SIGNAL
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_CHECK_TYPE(major_t, , AC_DEFINE(major_t, int,
				   [Type of major device numbers.]))
AC_CHECK_TYPE(minor_t, , AC_DEFINE(minor_t, int,
				   [Type of minor device numbers.]))
AC_CHECK_TYPE(dev_t, unsigned)
AC_CHECK_TYPE(ino_t, unsigned)

gt_TYPE_SSIZE_T
jm_AC_TYPE_UINTMAX_T


# gnulib modules

AM_FUNC_GETLINE

gl_BACKUPFILE
gl_DIRNAME
gl_ERROR
gl_EXCLUDE
gl_FILEBLOCKS
gl_FUNC_ALLOCA
gl_FUNC_FNMATCH_GNU
gl_FUNC_MEMSET
gl_FUNC_MKTIME
gl_FUNC_RMDIR
gl_FUNC_STRTOL
gl_FUNC_STRTOUL
gl_FUNC_STRTOIMAX
gl_FUNC_STRTOUMAX
gl_GETDATE
gl_GETOPT
gl_HASH
gl_HUMAN
gl_MODECHANGE
gl_OBSTACK
gl_PATHMAX
gl_QUOTE
gl_QUOTEARG
gl_SAFE_READ
gl_SAFE_WRITE
gl_SAVE_CWD
gl_SAVEDIR
gl_STRCASE
gl_TIME_R
gl_XALLOC
gl_XGETCWD
gl_XSTRTOL
gl_FUNC_STPCPY

jm_FUNC_FTRUNCATE
jm_FUNC_GLIBC_UNLOCKED_IO
jm_FUNC_CHOWN
jm_FUNC_LCHOWN
jm_FUNC_MALLOC
jm_FUNC_REALLOC
jm_FUNC_UTIME
jm_XSTRTOUMAX

AC_CHECK_MEMBERS([struct stat.st_spare1, struct stat.st_atim.tv_nsec, struct stat.st_atimespec.tv_nsec, struct stat.st_atimensec], , ,
                 [
#include <sys/types.h>
#include <sys/stat.h>])
 
# Save and restore LIBS so e.g., -lrt, isn't added to it.  Otherwise, *all*
# programs in the package would end up linked with that potentially-shared
# library, inducing unnecessary run-time overhead.

# Solaris 2.5.1 needs -lposix4 to get the clock_gettime function.
# Solaris 7 prefers the library name -lrt to the obsolescent name -lposix4.
tar_save_LIBS=$LIBS
  LIB_CLOCK_GETTIME=
  AC_SEARCH_LIBS(clock_gettime, [rt posix4])
  case "$ac_cv_search_clock_gettime" in
    -l*) LIB_CLOCK_GETTIME=$ac_cv_search_clock_gettime;;
  esac
  AC_SUBST(LIB_CLOCK_GETTIME)
  AC_CHECK_FUNCS(clock_gettime)
LIBS=$tar_save_LIBS

AC_CHECK_FUNCS(fsync lstat mkfifo readlink strerror symlink setlocale utimes)
AC_CHECK_DECLS([getgrgid],,, [#include <grp.h>])
AC_CHECK_DECLS([getpwuid],,, [#include <pwd.h>])
AC_CHECK_DECLS([valloc])

# Set LIB_SETSOCKOPT to -lnsl -lsocket if necessary.
tar_save_LIBS=$LIBS
  LIB_SETSOCKOPT=
  AC_SEARCH_LIBS(setsockopt, [socket], ,
    [AC_SEARCH_LIBS(setsockopt, [socket], , , [-lnsl])])
  AC_SEARCH_LIBS(setsockopt, [nsl])

  case "$ac_cv_search_setsockopt" in
    -l*) LIB_SETSOCKOPT=$ac_cv_search_setsockopt
  esac
  AC_SUBST(LIB_SETSOCKOPT)
LIBS=$tar_save_LIBS

AC_REPLACE_FUNCS(waitpid)

AC_CACHE_CHECK(for remote shell, tar_cv_path_RSH,
  [if test -n "$RSH"; then
    tar_cv_path_RSH=$RSH
  else
    tar_cv_path_RSH=no
    for ac_file in /usr/ucb/rsh /usr/bin/remsh /usr/bin/rsh /usr/bsd/rsh \
	/usr/bin/nsh /usr/bin/rcmd
    do
      # Prefer a non-symlink rsh to a symlink one, so that binaries built
      # on AIX 4.1.4, where /usr/ucb/rsh is a symlink to /usr/bin/rsh
      # will run on AIX 4.3.0, which has only /usr/bin/rsh.
      if test -f $ac_file; then
	if (test -h $ac_file) 2>/dev/null; then
	  test $tar_cv_path_RSH = no && tar_cv_path_RSH=$ac_file
	else
	  tar_cv_path_RSH=$ac_file
	  break
	fi
      fi
    done
  fi])
if test $tar_cv_path_RSH = no; then
  AC_CHECK_HEADERS(netdb.h)
else
  AC_DEFINE_UNQUOTED(REMOTE_SHELL, "$tar_cv_path_RSH",
    [Define to the full path of your rsh, if any.])
fi

AC_MSG_CHECKING(for default archive format)

if test -z "$DEFAULT_ARCHIVE_FORMAT"; then
  DEFAULT_ARCHIVE_FORMAT="GNU"
fi
case $DEFAULT_ARCHIVE_FORMAT in
  V7|OLDGNU|USTAR|POSIX|GNU) ;;
  *) AC_MSG_ERROR(Invalid format name);;
esac
AC_DEFINE_UNQUOTED(DEFAULT_ARCHIVE_FORMAT, ${DEFAULT_ARCHIVE_FORMAT}_FORMAT,
  [By default produce archives of this format])
AC_MSG_RESULT($DEFAULT_ARCHIVE_FORMAT)

AC_MSG_CHECKING(for default archive)

if test -z "$DEFAULT_ARCHIVE"; then
  DEFAULT_ARCHIVE=-
else
  if test -z "`ls $DEFAULT_ARCHIVE 2>/dev/null`"; then
    AC_MSG_WARN(DEFAULT_ARCHIVE \`$DEFAULT_ARCHIVE' not found on this system)
  fi
  # FIXME: Look for DEFTAPE in <sys/mtio.h>.
  # FIXME: Let DEVICE_PREFIX be configured from the environment.
  # FIXME: Rearrange, here.
  case $DEFAULT_ARCHIVE in
    *[[0-7][lmh]])
      AC_DEFINE(DENSITY_LETTER, 1,
	[[Define to 1 if density may be indicated by [lmh] at end of device.]])
      device_prefix=`echo $DEFAULT_ARCHIVE | sed 's/[0-7][lmh]$//'`
      ;;
    *[[0-7]])
      device_prefix=`echo $DEFAULT_ARCHIVE | sed 's/[0-7]$//'`
      ;;
    *)
      device_prefix=
      ;;
  esac
  case "$device_prefix" in
    ?*)
      AC_DEFINE_UNQUOTED(DEVICE_PREFIX, "$device_prefix",
	[Define to a string giving the prefix of the default device, without the part specifying the unit and density.])
      ;;
  esac
fi
AC_DEFINE_UNQUOTED(DEFAULT_ARCHIVE, "$DEFAULT_ARCHIVE",
  [Define to a string giving the full name of the default archive file.])
AC_MSG_RESULT($DEFAULT_ARCHIVE)

AC_MSG_CHECKING(for default blocking)
DEFAULT_BLOCKING=${DEFAULT_BLOCKING-20}
AC_DEFINE_UNQUOTED(DEFAULT_BLOCKING, $DEFAULT_BLOCKING,
  [Define to a number giving the default blocking size for archives.])
AC_MSG_RESULT($DEFAULT_BLOCKING)

# Gettext.
AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION(0.12.1)

AC_OUTPUT([Makefile\
           doc/Makefile\
           lib/Makefile\
           po/Makefile.in\
           scripts/Makefile\
           src/Makefile\
           tests/Makefile\
           tests/preset])

# Makefile for GNU tar library.

# Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2003 Free
# Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

noinst_LIBRARIES = libtar.a

libtar_a_SOURCES = prepargs.c prepargs.h

libtar_a_LIBADD = $(LIBOBJS) $(ALLOCA)
libtar_a_DEPENDENCIES = $(libtar_a_LIBADD)

BUILT_SOURCES =
EXTRA_DIST =
MAINTAINERCLEANFILES =
MOSTLYCLEANFILES =


# gnulib modules

# alloca
EXTRA_DIST += alloca_.h
all-local $(lib_OBJECTS): @ALLOCA_H@
alloca.h: alloca_.h
	cp $(srcdir)/alloca_.h alloca.h-t
	mv alloca.h-t alloca.h
MOSTLYCLEANFILES += alloca.h alloca.h-t

# argmatch
libtar_a_SOURCES += argmatch.h argmatch.c

# backupfile
libtar_a_SOURCES += backupfile.h backupfile.c addext.c

# dirname
libtar_a_SOURCES += dirname.h dirname.c basename.c stripslash.c

# exclude
libtar_a_SOURCES += exclude.h exclude.c

# fnmatch
EXTRA_DIST += fnmatch_.h fnmatch_loop.c
all-local $(lib_OBJECTS): @FNMATCH_H@
fnmatch.h: fnmatch_.h
	cp $(srcdir)/fnmatch_.h fnmatch.h-t
	mv fnmatch.h-t fnmatch.h
MOSTLYCLEANFILES += fnmatch.h fnmatch.h-t

# full-write
libtar_a_SOURCES += full-write.h full-write.c

# getdate
libtar_a_SOURCES += getdate.h getdate.y
BUILT_SOURCES += getdate.c
MAINTAINERCLEANFILES += getdate.c
#
# Say $(srcdir), so GNU make does not report an ambiguity with the .y.c rule.
$(srcdir)/getdate.c: getdate.y
	cd $(srcdir) && \
	  $(YACC) $(YFLAGS) getdate.y && \
	  mv -f y.tab.c getdate.c

# getline
libtar_a_SOURCES += getline.h

# getopt
libtar_a_SOURCES += getopt.h getopt.c getopt1.c

# gettext
libtar_a_SOURCES += gettext.h

# hash
libtar_a_SOURCES += hash.h hash.c

# human
libtar_a_SOURCES += human.h human.c

# exitfail
libtar_a_SOURCES += exitfail.h exitfail.c

# lchown
libtar_a_SOURCES += lchown.h

# modechange
libtar_a_SOURCES += modechange.h modechange.c

# pathmax
libtar_a_SOURCES += pathmax.h

# quote
libtar_a_SOURCES += quote.h quote.c

# quotearg
libtar_a_SOURCES += quotearg.h quotearg.c

# safe-read
libtar_a_SOURCES += safe-read.h safe-read.c

# safe-write
libtar_a_SOURCES += safe-write.h safe-write.c

# save-cwd
libtar_a_SOURCES += save-cwd.h save-cwd.c

# savedir
libtar_a_SOURCES += savedir.h savedir.c

# stdbool
EXTRA_DIST += stdbool_.h
all-local $(lib_OBJECTS): $(STDBOOL_H)
stdbool.h: stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/$(HAVE__BOOL)/g' < $(srcdir)/stdbool_.h > stdbool.h-t
	mv stdbool.h-t stdbool.h
MOSTLYCLEANFILES += stdbool.h stdbool.h-t

# unlocked-io
libtar_a_SOURCES += unlocked-io.h

# xalloc
libtar_a_SOURCES += xalloc.h xmalloc.c xstrdup.c

# xgetcwd
libtar_a_SOURCES += xgetcwd.h xgetcwd.c

# xstrtol
libtar_a_SOURCES += xstrtol.h xstrtol.c xstrtoul.c

# xstrtoumax
libtar_a_SOURCES += xstrtoumax.c

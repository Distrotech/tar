# Makefile for GNU tar documentation.

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
## along with this program; if not, write to the Free Software Foundation,
## Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

info_TEXINFOS = tar.texi
tar_TEXINFOS = fdl.texi freemanuals.texi getdate.texi header.texi rendition.texi value.texi
EXTRA_DIST = convtexi.pl gendocs_template 
DISTCLEANFILES=*.info* 

# The rendering level is anyone of PUBLISH, DISTRIB or PROOF.
# Just call `make RENDITION=PROOF [target]' if you want PROOF rendition.
RENDITION = DISTRIB

MAKEINFOFLAGS=-D$(RENDITION)

header.texi: $(top_srcdir)/src/tar.h
	sed -n '/Archive Format/,/End of Format/p' $(top_srcdir)/src/tar.h \
	  | expand | sed 's/\([{}]\)/@\1/g' >$@

clean-local:
	rm -rf manual

GENDOCS=gendocs.sh

TEXI2DVI=texi2dvi -t '@set $(RENDITION)' -E

# Make sure you set TEXINPUTS
manual:
	TEXINPUTS=$(srcdir):$(top_srcdir)/config:$(TEXINPUTS) \
	 MAKEINFO="$(MAKEINFO) $(MAKEINFOFLAGS)" \
	 TEXI2DVI="$(TEXI2DVI) -t @finalout" \
	$(GENDOCS) tar 'GNU tar manual'


#! /bin/sh
. ./preset
star_prereq ustar-big-2g.tar.bz2
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvjf $STAR_TESTSCRIPTS/ustar-big-2g.tar.bz2

out="\
-rw------- jes/glone 2147483647 2002-06-15 14:53:23 big
-rw-r--r-- jes/glone          0 2002-06-15 14:53:32 file
"

# Fixme: why?
err="\
tar: Read 4096 bytes from $STAR_TESTSCRIPTS/ustar-big-2g.tar.bz2
"

. $srcdir/after

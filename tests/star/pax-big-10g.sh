#! /bin/sh
. ./preset
star_prereq pax-big-10g.tar.bz2
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvjf $STAR_TESTSCRIPTS/pax-big-10g.tar.bz2

out="\
-rw------- jes/glone 10737418240 2002-06-15 21:18:47 10g
-rw-r--r-- jes/glone           0 2002-06-15 14:53:32 file
"

err="\
tar: Read 3072 bytes from $STAR_TESTSCRIPTS/pax-big-10g.tar.bz2
"

. $srcdir/after

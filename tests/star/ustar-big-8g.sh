#! /bin/sh
. ./preset
star_prereq ustar-big-8g.tar.bz2
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvjf $STAR_TESTSCRIPTS/ustar-big-8g.tar.bz2

out="\
-rw------- jes/glone 8589934591 2002-06-15 15:08:33 8gb-1
-rw-r--r-- jes/glone          0 2002-06-15 14:53:32 file
"

err_regex="\
tar: Record size = .*
"

. $srcdir/after

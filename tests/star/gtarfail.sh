#! /bin/sh
. ./preset
star_prereq gtarfail.tar
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvf $STAR_TESTSCRIPTS/gtarfail.tar

out="\
-rw-r--r-- jes/glone       518 2001-05-25 14:41:06 vedpowered.gif
-rw-r--r-- jes/glone      6825 1997-04-29 00:19:16 cd.gif
-rw-r--r-- jes/glone     33354 1999-06-22 12:17:38 DSCN0049c.JPG
-rw-r--r-- jes/glone     86159 2001-06-05 18:16:04 Window1.jpg
-rw-r--r-- jes/glone      1310 2001-05-25 13:05:41 vipower.gif
-rw-rw-rw- jes/glone    148753 1998-09-15 13:08:15 billyboy.jpg
"
. $srcdir/after

#! /bin/sh
. ./preset
star_prereq gtarfail2.tar
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

tar --utc -tvf $STAR_TESTSCRIPTS/gtarfail2.tar

out="\
-rwxr-xr-x jes/glone       214 2001-09-21 14:08:36 .clean
lrwxrwxrwx jes/cats          0 1998-05-07 12:39:00 RULES -> makefiles/RULES
drwxr-sr-x jes/glone         0 2001-12-10 00:00:58 build/
-rw-r--r-- jes/glone    312019 2001-12-10 00:00:20 build/smake-1.2.tar.gz
drwxr-sr-x jes/glone         0 2001-11-09 18:20:33 build/psmake/
-rwxr-xr-x jes/glone       259 2000-01-09 16:36:34 build/psmake/MAKE
-rwxr-xr-x jes/glone      4820 2001-02-25 22:45:53 build/psmake/MAKE.sh
-rw-r--r-- jes/glone       647 2001-02-25 23:50:06 build/psmake/Makefile
lrwxrwxrwx jes/glone         0 2001-08-29 10:53:53 build/psmake/archconf.c -> ../archconf.c
lrwxrwxrwx jes/glone         0 2001-08-29 10:54:00 build/psmake/astoi.c -> ../../lib/astoi.c
"

. $srcdir/after

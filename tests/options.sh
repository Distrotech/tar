#! /bin/sh
# Ensure that TAR_OPTIONS works in conjunction with old-style options.

. ./preset
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

set -e
echo > file1
TAR_OPTIONS=--numeric-owner tar chof archive file1
tar tf archive

out="\
file1
"

. $srcdir/after

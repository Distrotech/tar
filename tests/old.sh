#! /bin/sh
# An old archive was not receiving directories.

. ./preset
TAR_ARCHIVE_FORMATS=auto
. $srcdir/before

set -e
mkdir directory
tar cfvo archive directory
tar tf archive

out="\
directory/
directory/
"

. $srcdir/after

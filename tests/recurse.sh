#! /bin/sh

. ./preset
. $srcdir/before

set -e
mkdir directory
touch directory/file
tar --create --file archive --no-recursion directory || exit 1
tar tf archive

out="directory/
"

. $srcdir/after

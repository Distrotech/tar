#! /bin/sh
# Deleting a large last member was destroying earlier members.

. ./preset
. $srcdir/before

set -e
genfile -l      3 >file1
genfile -l      5 >file2
genfile -l      3 >file3
genfile -l      6 >file4
genfile -l     24 >file5
genfile -l     13 >file6
genfile -l   1385 >file7
genfile -l     30 >file8
genfile -l     10 >file9
genfile -l 256000 >file10
tar cf archive file1 file2 file3 file4 file5 file6 file7 file8 file9 file10
tar f archive --delete file10
tar tf archive

out="\
file1
file2
file3
file4
file5
file6
file7
file8
file9
"

. $srcdir/after

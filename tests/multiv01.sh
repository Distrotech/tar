#! /bin/sh
# Test multivolume dumps from pipes.

. ./preset
. $srcdir/before

set -e

dd if=/dev/zero bs=1024 count=7 >file1

for block in " 1" " 2" " 3" " 4" " 5" " 6" " 7" " 8" \
              " 9" "10" "11" "12" "13" "14" "15" "16" ; do \
  echo "file2  block ${block} bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla"
  for count in 2 3 4 5 6 7 8 ; do
    echo "bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla!bla"
  done
done >file2

tar -c --multi-volume --tape-length=10 \
  --listed-incremental=t.snar \
  -f t1-pipe.tar -f t2-pipe.tar ./file1 ./file2

mkdir extract-dir-pipe
dd bs=4096 count=10 <t2-pipe.tar |
PATH=$PATH truss -o /tmp/tr tar -f t1-pipe.tar -f - -C extract-dir-pipe -x --multi-volume \
  --tape-length=10 --read-full-records

cmp file1 extract-dir-pipe/file1
cmp file2 extract-dir-pipe/file2

out="\
"
err="\
7+0 records in
7+0 records out
"

. $srcdir/after

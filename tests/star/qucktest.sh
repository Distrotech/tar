#! /bin/sh
. ./preset
star_prereq ustar-all-quicktest.tar
star_prereq quicktest.filelist
# Only root may perform this test
test -w / || skiptest

TAR_ARCHIVE_FORMATS=ustar
. $srcdir/before

mkdir directory
cd directory

save_TAR_OPTIONS=$TAR_OPTIONS
TAR_OPTIONS="" tar xf $STAR_TESTSCRIPTS/ustar-all-quicktest.tar
TAR_OPTIONS=$save_TAR_OPTIONS
echo separator
echo separator >&2
tar cfT ../archive $STAR_TESTSCRIPTS/quicktest.filelist
cd ..

${TARTEST:-tartest} -v < $STAR_TESTSCRIPTS/ustar-all-quicktest.tar > old.out
${TARTEST:-tartest} -v < archive > new.out

cmp old.out new.out

out="\
separator
"

err_ignore="tar: Extracting contiguous files as regular files"

err="\
separator
"
					
. $srcdir/after

## Process this file with automake to produce Makefile.in -*-Makefile-*-

##m4-files-begin
EXTRA_DIST = README Makefile.am.in \
ccstdc.m4 \
d-ino.m4 \
gettext.m4 \
inttypes_h.m4 \
isc-posix.m4 \
jm-mktime.m4 \
largefile.m4 \
lcmessage.m4 \
malloc.m4 \
progtest.m4 \
realloc.m4 \
uintmax_t.m4 \
ulonglong.m4 \
utimbuf.m4 \
utime.m4 \
utimes.m4 \
xstrtoumax.m4
##m4-files-end

Makefile.am: Makefile.am.in
	rm -f $@ $@t
	sed -n '1,/^##m4-files-begin/p' $< > $@t
	( (echo EXTRA_DIST = README Makefile.am.in | tr '\012' @); \
	  (echo *.m4|tr ' ' @) ) \
	  |sed 's/@$$/%/;s/@/ \\@/g' |tr @% '\012\012' \
	  >> $@t
	sed -n '/^##m4-files-end/,$$p' $< >> $@t
	chmod a-w $@t
	mv $@t $@

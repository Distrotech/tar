dnl This is a copy of autoconf 2.13, except we also check that
dnl FNM_FILE_NAME | FNM_LEADING_DIR works.
dnl
undefine([AC_FUNC_FNMATCH])
AC_DEFUN(AC_FUNC_FNMATCH,
[AC_CACHE_CHECK(for GNU-style fnmatch, ac_cv_func_fnmatch_works,
# Some versions of Solaris, SCO, and the GNU C Library
# have a broken or incompatible fnmatch.
# So we run a test program.  If we are cross-compiling, take no chance.
# Thanks to John Oleynick, Franc,ois Pinard, and Paul Eggert for this test.
[AC_TRY_RUN([#include <fnmatch.h>
main() {
  exit (fnmatch ("a*", "abc", 0) != 0
	|| fnmatch("*", "x", FNM_FILE_NAME | FNM_LEADING_DIR) != 0
	|| fnmatch("x*", "x/y/z", FNM_FILE_NAME | FNM_LEADING_DIR) != 0
	|| fnmatch("*c*", "c/x", FNM_FILE_NAME | FNM_LEADING_DIR) != 0);
}],
ac_cv_func_fnmatch_works=yes, ac_cv_func_fnmatch_works=no,
ac_cv_func_fnmatch_works=no)])
if test $ac_cv_func_fnmatch_works = yes; then
  AC_DEFINE(HAVE_FNMATCH)
fi
])

#serial 1

dnl From Paul Eggert

AC_DEFUN(jm_FUNC_MBRTOWC,
[
  AC_MSG_CHECKING([whether mbrtowc is declared])
  AC_CACHE_VAL(jm_cv_func_mbrtowc,
    [AC_TRY_LINK(
       [#include <wchar.h>],
       [return !mbrtowc;],
       [jm_cv_func_mbrtowc=yes],
       [jm_cv_func_mbrtowc=no])])
  if test $jm_cv_func_mbrtowc = yes; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_MBRTOWC, 1,
      [Define to 1 if mbrtowc exists and is properly declared.])
  fi
])

# ifndef PARAMS
#  if PROTOTYPES || (defined (__STDC__) && __STDC__)
#   define PARAMS(args) args
#  else
#   define PARAMS(args) ()
#  endif
# endif

char const *copyright_symbol PARAMS((char *, size_t));

#ifndef UNICODEIO_H
# define UNICODEIO_H

# include <stdio.h>

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

/* Converts the Unicode character CODE to its multibyte representation
   in the current locale and calls the CALLBACK on the resulting byte
   sequence.  */
extern void unicode_to_mb
	    PARAMS ((unsigned int code,
		     void (*callback) PARAMS ((const char *buf, size_t buflen,
					       void *callback_arg)),
		     void *callback_arg));

/* Outputs the Unicode character CODE to the output stream STREAM.  */
extern void print_unicode_char PARAMS((FILE *stream, unsigned int code));

#endif

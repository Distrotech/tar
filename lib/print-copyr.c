/* copysym.c -- Return a copyright symbol suitable for the current locale.
   Copyright (C) 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_ICONV
# include <iconv.h>

# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif

#include "copysym.h"

/* Store into BUF (of size BUFSIZE) a representation of the copyright
   symbol (C-in-a-circle) that is a valid text string for the current
   locale.  Return BUF if successful, and a pointer to some other
   string otherwise.  */

char const *
copyright_symbol (char *buf, size_t bufsize)
{
#if HAVE_ICONV
  char const *outcharset = getenv ("OUTPUT_CHARSET");

  if (! (outcharset && *outcharset))
    {
      extern char const *locale_charset (void);
      outcharset = locale_charset ();
    }

  if (*outcharset)
    {
      iconv_t conv = iconv_open (outcharset, "UTF-8");

      if (conv != (iconv_t) -1)
	{
	  static char const copyright_utf_8[] = "\302\251";
	  char ICONV_CONST *inptr = (char ICONV_CONST *) &copyright_utf_8;
	  size_t inleft = sizeof copyright_utf_8;
	  char *outptr = buf;
	  size_t chars = iconv (conv, &inptr, &inleft, &outptr, &bufsize);

	  iconv_close (conv);

	  if (chars != (size_t) -1)
	    return buf;
	}
    }
#endif

  /* "(C)" is the best we can do in ASCII.  */
  return "(C)";
}

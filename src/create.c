/* Create a tar archive.

   Copyright (C) 1985, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001,
   2003 Free Software Foundation, Inc.

   Written by John Gilmore, on 1985-08-25.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

#if HAVE_UTIME_H
# include <utime.h>
#else
struct utimbuf
  {
    long actime;
    long modtime;
  };
#endif

#include <quotearg.h>

#include "common.h"
#include <hash.h>

struct link
  {
    dev_t dev;
    ino_t ino;
    size_t nlink;
    char name[1];
  };

/* The maximum uintmax_t value that can be represented with DIGITS digits,
   assuming that each digit is BITS_PER_DIGIT wide.  */
#define MAX_VAL_WITH_DIGITS(digits, bits_per_digit) \
   ((digits) * (bits_per_digit) < sizeof (uintmax_t) * CHAR_BIT \
    ? ((uintmax_t) 1 << ((digits) * (bits_per_digit))) - 1 \
    : (uintmax_t) -1)

/* Convert VALUE to an octal representation suitable for tar headers.
   Output to buffer WHERE with size SIZE.
   The result is undefined if SIZE is 0 or if VALUE is too large to fit.  */

static void
to_octal (uintmax_t value, char *where, size_t size)
{
  uintmax_t v = value;
  size_t i = size;

  do
    {
      where[--i] = '0' + (v & ((1 << LG_8) - 1));
      v >>= LG_8;
    }
  while (i);
}

/* Convert NEGATIVE VALUE to a base-256 representation suitable for
   tar headers.  NEGATIVE is 1 if VALUE was negative before being cast
   to uintmax_t, 0 otherwise.  Output to buffer WHERE with size SIZE.
   The result is undefined if SIZE is 0 or if VALUE is too large to
   fit.  */

static void
to_base256 (int negative, uintmax_t value, char *where, size_t size)
{
  uintmax_t v = value;
  uintmax_t propagated_sign_bits =
    ((uintmax_t) - negative << (CHAR_BIT * sizeof v - LG_256));
  size_t i = size;

  do
    {
      where[--i] = v & ((1 << LG_256) - 1);
      v = propagated_sign_bits | (v >> LG_256);
    }
  while (i);
}

/* Convert NEGATIVE VALUE (which was originally of size VALSIZE) to
   external form, using SUBSTITUTE (...) if VALUE won't fit.  Output
   to buffer WHERE with size SIZE.  NEGATIVE is 1 iff VALUE was
   negative before being cast to uintmax_t; its original bitpattern
   can be deduced from VALSIZE, its original size before casting.
   TYPE is the kind of value being output (useful for diagnostics).
   Prefer the POSIX format of SIZE - 1 octal digits (with leading zero
   digits), followed by '\0'.  If this won't work, and if GNU or
   OLDGNU format is allowed, use '\200' followed by base-256, or (if
   NEGATIVE is nonzero) '\377' followed by two's complement base-256.
   If neither format works, use SUBSTITUTE (...)  instead.  Pass to
   SUBSTITUTE the address of an 0-or-1 flag recording whether the
   substitute value is negative.  */

static void
to_chars (int negative, uintmax_t value, size_t valsize,
	  uintmax_t (*substitute) (int *),
	  char *where, size_t size, const char *type)
{
  int base256_allowed = (archive_format == GNU_FORMAT
			 || archive_format == OLDGNU_FORMAT);

  /* Generate the POSIX octal representation if the number fits.  */
  if (! negative && value <= MAX_VAL_WITH_DIGITS (size - 1, LG_8))
    {
      where[size - 1] = '\0';
      to_octal (value, where, size - 1);
    }

  /* Otherwise, generate the base-256 representation if we are
     generating an old or new GNU format and if the number fits.  */
  else if (((negative ? -1 - value : value)
	    <= MAX_VAL_WITH_DIGITS (size - 1, LG_256))
	   && base256_allowed)
    {
      where[0] = negative ? -1 : 1 << (LG_256 - 1);
      to_base256 (negative, value, where + 1, size - 1);
    }

  /* Otherwise, if the number is negative, and if it would not cause
     ambiguity on this host by confusing positive with negative
     values, then generate the POSIX octal representation of the value
     modulo 2**(field bits).  The resulting tar file is
     machine-dependent, since it depends on the host word size.  Yuck!
     But this is the traditional behavior.  */
  else if (negative && valsize * CHAR_BIT <= (size - 1) * LG_8)
    {
      static int warned_once;
      if (! warned_once)
	{
	  warned_once = 1;
	  WARN ((0, 0, _("Generating negative octal headers")));
	}
      where[size - 1] = '\0';
      to_octal (value & MAX_VAL_WITH_DIGITS (valsize * CHAR_BIT, 1),
		where, size - 1);
    }

  /* Otherwise, output a substitute value if possible (with a
     warning), and an error message if not.  */
  else
    {
      uintmax_t maxval = (base256_allowed
			  ? MAX_VAL_WITH_DIGITS (size - 1, LG_256)
			  : MAX_VAL_WITH_DIGITS (size - 1, LG_8));
      char valbuf[UINTMAX_STRSIZE_BOUND + 1];
      char maxbuf[UINTMAX_STRSIZE_BOUND];
      char minbuf[UINTMAX_STRSIZE_BOUND + 1];
      char const *minval_string;
      char const *maxval_string = STRINGIFY_BIGINT (maxval, maxbuf);
      char const *value_string;

      if (base256_allowed)
	{
	  uintmax_t m = maxval + 1 ? maxval + 1 : maxval / 2 + 1;
	  char *p = STRINGIFY_BIGINT (m, minbuf + 1);
	  *--p = '-';
	  minval_string = p;
	}
      else
	minval_string = "0";

      if (negative)
	{
	  char *p = STRINGIFY_BIGINT (- value, valbuf + 1);
	  *--p = '-';
	  value_string = p;
	}
      else
	value_string = STRINGIFY_BIGINT (value, valbuf);

      if (substitute)
	{
	  int negsub;
	  uintmax_t sub = substitute (&negsub) & maxval;
	  uintmax_t s = (negsub &= archive_format == GNU_FORMAT) ? - sub : sub;
	  char subbuf[UINTMAX_STRSIZE_BOUND + 1];
	  char *sub_string = STRINGIFY_BIGINT (s, subbuf + 1);
	  if (negsub)
	    *--sub_string = '-';
	  WARN ((0, 0, _("value %s out of %s range %s..%s; substituting %s"),
		 value_string, type, minval_string, maxval_string,
		 sub_string));
	  to_chars (negsub, s, valsize, 0, where, size, type);
	}
      else
	ERROR ((0, 0, _("value %s out of %s range %s..%s"),
		value_string, type, minval_string, maxval_string));
    }
}

static uintmax_t
gid_substitute (int *negative)
{
  gid_t r;
#ifdef GID_NOBODY
  r = GID_NOBODY;
#else
  static gid_t gid_nobody;
  if (!gid_nobody && !gname_to_gid ("nobody", &gid_nobody))
    gid_nobody = -2;
  r = gid_nobody;
#endif
  *negative = r < 0;
  return r;
}

void
gid_to_chars (gid_t v, char *p, size_t s)
{
  to_chars (v < 0, (uintmax_t) v, sizeof v, gid_substitute, p, s, "gid_t");
}

void
major_to_chars (major_t v, char *p, size_t s)
{
  to_chars (v < 0, (uintmax_t) v, sizeof v, 0, p, s, "major_t");
}

void
minor_to_chars (minor_t v, char *p, size_t s)
{
  to_chars (v < 0, (uintmax_t) v, sizeof v, 0, p, s, "minor_t");
}

void
mode_to_chars (mode_t v, char *p, size_t s)
{
  /* In the common case where the internal and external mode bits are the same,
     and we are not using POSIX or GNU format,
     propagate all unknown bits to the external mode.
     This matches historical practice.
     Otherwise, just copy the bits we know about.  */
  int negative;
  uintmax_t u;
  if (S_ISUID == TSUID && S_ISGID == TSGID && S_ISVTX == TSVTX
      && S_IRUSR == TUREAD && S_IWUSR == TUWRITE && S_IXUSR == TUEXEC
      && S_IRGRP == TGREAD && S_IWGRP == TGWRITE && S_IXGRP == TGEXEC
      && S_IROTH == TOREAD && S_IWOTH == TOWRITE && S_IXOTH == TOEXEC
      && archive_format != POSIX_FORMAT
      && archive_format != GNU_FORMAT)
    {
      negative = v < 0;
      u = v;
    }
  else
    {
      negative = 0;
      u = ((v & S_ISUID ? TSUID : 0)
	   | (v & S_ISGID ? TSGID : 0)
	   | (v & S_ISVTX ? TSVTX : 0)
	   | (v & S_IRUSR ? TUREAD : 0)
	   | (v & S_IWUSR ? TUWRITE : 0)
	   | (v & S_IXUSR ? TUEXEC : 0)
	   | (v & S_IRGRP ? TGREAD : 0)
	   | (v & S_IWGRP ? TGWRITE : 0)
	   | (v & S_IXGRP ? TGEXEC : 0)
	   | (v & S_IROTH ? TOREAD : 0)
	   | (v & S_IWOTH ? TOWRITE : 0)
	   | (v & S_IXOTH ? TOEXEC : 0));
    }
  to_chars (negative, u, sizeof v, 0, p, s, "mode_t");
}

void
off_to_chars (off_t v, char *p, size_t s)
{
  to_chars (v < 0, (uintmax_t) v, sizeof v, 0, p, s, "off_t");
}

void
size_to_chars (size_t v, char *p, size_t s)
{
  to_chars (0, (uintmax_t) v, sizeof v, 0, p, s, "size_t");
}

void
time_to_chars (time_t v, char *p, size_t s)
{
  to_chars (v < 0, (uintmax_t) v, sizeof v, 0, p, s, "time_t");
}

static uintmax_t
uid_substitute (int *negative)
{
  uid_t r;
#ifdef UID_NOBODY
  r = UID_NOBODY;
#else
  static uid_t uid_nobody;
  if (!uid_nobody && !uname_to_uid ("nobody", &uid_nobody))
    uid_nobody = -2;
  r = uid_nobody;
#endif
  *negative = r < 0;
  return r;
}

void
uid_to_chars (uid_t v, char *p, size_t s)
{
  to_chars (v < 0, (uintmax_t) v, sizeof v, uid_substitute, p, s, "uid_t");
}

void
uintmax_to_chars (uintmax_t v, char *p, size_t s)
{
  to_chars (0, v, sizeof v, 0, p, s, "uintmax_t");
}

void
string_to_chars (char *str, char *p, size_t s)
{
  strncpy (p, str, s);
  p[s-1] = 0;
}


/* Writing routines.  */

/* Zero out the buffer so we don't confuse ourselves with leftover
   data.  */
static void
clear_buffer (char *buffer)
{
  memset (buffer, 0, BLOCKSIZE);
}

/* Write the EOT block(s).  Zero at least two blocks, through the end
   of the record.  Old tar, as previous versions of GNU tar, writes
   garbage after two zeroed blocks.  */
void
write_eot (void)
{
  union block *pointer = find_next_block ();
  memset (pointer->buffer, 0, BLOCKSIZE);
  set_next_block_after (pointer);
  pointer = find_next_block ();
  memset (pointer->buffer, 0, available_space_after (pointer));
  set_next_block_after (pointer);
}

/* Copy at most LEN bytes from SRC to DST. Terminate with NUL unless
   SRC is LEN characters long */
static void
tar_copy_str (char *dst, const char *src, size_t len)
{
  dst[len-1] = 0;
  strncpy (dst, src, len);
}

/* Write a "private" header */
static union block *
start_private_header (const char *name, size_t size)
{
  time_t t;
  union block *header = find_next_block ();
  
  memset (header->buffer, 0, sizeof (union block));

  tar_copy_str (header->header.name, name, NAME_FIELD_SIZE);
  OFF_TO_CHARS (size, header->header.size);

  time (&t);
  TIME_TO_CHARS (t, header->header.mtime);
  MODE_TO_CHARS (S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH, header->header.mode);
  UID_TO_CHARS (getuid (), header->header.uid);
  GID_TO_CHARS (getgid (), header->header.gid);
  MAJOR_TO_CHARS (0, header->header.devmajor);
  MAJOR_TO_CHARS (0, header->header.devminor);
  strncpy (header->header.magic, TMAGIC, TMAGLEN);
  strncpy (header->header.version, TVERSION, TVERSLEN);
  return header;
}

/* Create a new header and store there at most NAME_FIELD_SIZE bytes of
   the file name */

static union block * 
write_short_name (struct tar_stat_info *st)
{
  union block *header = find_next_block ();
  memset (header->buffer, 0, sizeof (union block));
  tar_copy_str (header->header.name, st->file_name, NAME_FIELD_SIZE);
  return header;
}

/* Write a GNUTYPE_LONGLINK or GNUTYPE_LONGNAME block.  */
static void
write_gnu_long_link (const char *p, char type)
{
  size_t size = strlen (p) + 1;
  size_t bufsize;
  union block *header;

  header = start_private_header ("././@LongLink", size);
  header->header.typeflag = type;
  finish_header (header, -1);

  header = find_next_block ();

  bufsize = available_space_after (header);

  while (bufsize < size)
    {
      memcpy (header->buffer, p, bufsize);
      p += bufsize;
      size -= bufsize;
      set_next_block_after (header + (bufsize - 1) / BLOCKSIZE);
      header = find_next_block ();
      bufsize = available_space_after (header);
    }
  memcpy (header->buffer, p, size);
  memset (header->buffer + size, 0, bufsize - size);
  set_next_block_after (header + (size - 1) / BLOCKSIZE);
}

static size_t
split_long_name (const char *name, size_t length)
{
  size_t i;

  if (length > PREFIX_FIELD_SIZE)
    length = PREFIX_FIELD_SIZE+2;
  for (i = length - 1; i > 0; i--)
    if (ISSLASH (name[i]))
      break;
  return i;
}

static union block *
write_ustar_long_name (const char *name)
{
  size_t length = strlen (name);
  size_t i;
  union block *header;

  if (length > PREFIX_FIELD_SIZE + NAME_FIELD_SIZE + 1)
    {
      WARN ((0, 0, _("%s: file name is too long (max %d); not dumped"),
	     quotearg_colon (name),
	     PREFIX_FIELD_SIZE + NAME_FIELD_SIZE + 1));
      return NULL;
    }
  
  i = split_long_name (name, length);
  if (i == 0 || length - i -1 > NAME_FIELD_SIZE)
    {
      WARN ((0, 0,
	     _("%s: file name is too long (cannot be split); not dumped"),
	     quotearg_colon (name),
	     PREFIX_FIELD_SIZE + NAME_FIELD_SIZE + 1));
      return NULL;
    }

  header = find_next_block ();
  memset (header->buffer, 0, sizeof (header->buffer));
  memcpy (header->header.prefix, name, i);
  memcpy (header->header.name, name + i + 1, length - i - 1);
  
  return header;
}

/* Write a long link name, depending on the current archive format */
static void
write_long_link (struct tar_stat_info *st)
{
  switch (archive_format)
    {
    case POSIX_FORMAT:
      xheader_store ("linkpath", st);
      break;

    case V7_FORMAT:			/* old V7 tar format */
    case USTAR_FORMAT:
    case STAR_FORMAT:
      WARN ((0, 0,
	     _("%s: link name is too long; not dumped"),
	     quotearg_colon (st->link_name)));
	    break;
      
    case OLDGNU_FORMAT:
    case GNU_FORMAT:
      write_gnu_long_link (st->link_name, GNUTYPE_LONGLINK);
      break;

    default:
      abort(); /*FIXME*/
    }
}

static union block *
write_long_name (struct tar_stat_info *st)
{
  switch (archive_format)
    {
    case POSIX_FORMAT:
      xheader_store ("path", st);
      break;

    case V7_FORMAT:		      
    case USTAR_FORMAT:
    case STAR_FORMAT:
      return write_ustar_long_name (st->file_name);
      
    case OLDGNU_FORMAT:
    case GNU_FORMAT:
      write_gnu_long_link (st->file_name, GNUTYPE_LONGNAME);
      break;

    default:
      abort(); /*FIXME*/
    }
  return write_short_name (st);
}

static union block *
write_extended (union block *old_header, char type)
{
  union block *header, hp;
  struct tar_stat_info foo;
  size_t size;
  char *p;

  if (extended_header.buffer || extended_header.stk == NULL)
    return old_header; 
  
  xheader_finish (&extended_header);
  size = extended_header.size;

  memcpy (hp.buffer, old_header, sizeof (hp));
  
  header = start_private_header ("././@PaxHeader", size);
  header->header.typeflag = type;

  finish_header (header, -1);

  p = extended_header.buffer;

  do
    {
      size_t len;
      
      header = find_next_block ();
      len = BLOCKSIZE;
      if (len > size)
	len = size;
      memcpy (header->buffer, p, len);
      if (len < BLOCKSIZE)
	memset (header->buffer + len, 0, BLOCKSIZE - len);
      p += len;
      size -= len;
      set_next_block_after (header);
    }
  while (size > 0);
  
  xheader_destroy (&extended_header);
  header = find_next_block ();
  memcpy (header, &hp.buffer, sizeof (hp.buffer));
  return header;
}

static union block * 
write_header_name (struct tar_stat_info *st)
{
  if (NAME_FIELD_SIZE < strlen (st->file_name))
    return write_long_name (st);
  else
    return write_short_name (st);
}


/* Header handling.  */

/* Make a header block for the file whose stat info is st,
   and return its address.  */

static union block *
start_header (const char *name, struct tar_stat_info *st)
{
  union block *header;

  name = safer_name_suffix (name, 0);
  assign_string (&st->file_name, name);

  header = write_header_name (st);
  if (!header)
    return NULL;

  assign_string (&current_stat_info.file_name, name);

  /* Override some stat fields, if requested to do so.  */

  if (owner_option != (uid_t) -1)
    st->stat.st_uid = owner_option;
  if (group_option != (gid_t) -1)
    st->stat.st_gid = group_option;
  if (mode_option)
    st->stat.st_mode = ((st->stat.st_mode & ~MODE_ALL)
		   | mode_adjust (st->stat.st_mode, mode_option));

  /* Paul Eggert tried the trivial test ($WRITER cf a b; $READER tvf a)
     for a few tars and came up with the following interoperability
     matrix:

	      WRITER
	1 2 3 4 5 6 7 8 9   READER
	. . . . . . . . .   1 = SunOS 4.2 tar
	# . . # # . . # #   2 = NEC SVR4.0.2 tar
	. . . # # . . # .   3 = Solaris 2.1 tar
	. . . . . . . . .   4 = GNU tar 1.11.1
	. . . . . . . . .   5 = HP-UX 8.07 tar
	. . . . . . . . .   6 = Ultrix 4.1
	. . . . . . . . .   7 = AIX 3.2
	. . . . . . . . .   8 = Hitachi HI-UX 1.03
	. . . . . . . . .   9 = Omron UNIOS-B 4.3BSD 1.60Beta

	     . = works
	     # = ``impossible file type''

     The following mask for old archive removes the `#'s in column 4
     above, thus making GNU tar both a universal donor and a universal
     acceptor for Paul's test.  */

  if (archive_format == V7_FORMAT || archive_format == USTAR_FORMAT)
    MODE_TO_CHARS (st->stat.st_mode & MODE_ALL, header->header.mode);
  else
    MODE_TO_CHARS (st->stat.st_mode, header->header.mode);

  if (st->stat.st_uid > MAXOCTAL7 && archive_format == POSIX_FORMAT)
    xheader_store ("uid", st);
  else
    UID_TO_CHARS (st->stat.st_uid, header->header.uid);
  
  if (st->stat.st_gid > MAXOCTAL7 && archive_format == POSIX_FORMAT)
    xheader_store ("gid", st);
  else
    GID_TO_CHARS (st->stat.st_gid, header->header.gid);

  if (st->stat.st_size > MAXOCTAL11 && archive_format == POSIX_FORMAT)
    xheader_store ("size", st);
  else
    OFF_TO_CHARS (st->stat.st_size, header->header.size);

  TIME_TO_CHARS (st->stat.st_mtime, header->header.mtime);

  /* FIXME */
  if (S_ISCHR (st->stat.st_mode)
      || S_ISBLK (st->stat.st_mode))
    {
      st->devmajor = major (st->stat.st_rdev);
      st->devminor = minor (st->stat.st_rdev);

      if (st->devmajor > MAXOCTAL7 && archive_format == POSIX_FORMAT)
	xheader_store ("devmajor", st);
      else
	MAJOR_TO_CHARS (st->devmajor, header->header.devmajor);

      if (st->devminor > MAXOCTAL7 && archive_format == POSIX_FORMAT)
	xheader_store ("devminor", st);
      else
	MAJOR_TO_CHARS (st->devminor, header->header.devminor);
    }
  else
    {
      MAJOR_TO_CHARS (0, header->header.devmajor);
      MINOR_TO_CHARS (0, header->header.devminor);
    }
  
  if (archive_format == POSIX_FORMAT)
    {
      xheader_store ("atime", st);
      xheader_store ("ctime", st);
    }
  else if (incremental_option)
    if (archive_format == OLDGNU_FORMAT)
      {
	TIME_TO_CHARS (st->stat.st_atime, header->oldgnu_header.atime);
	TIME_TO_CHARS (st->stat.st_ctime, header->oldgnu_header.ctime);
      }

  header->header.typeflag = archive_format == V7_FORMAT ? AREGTYPE : REGTYPE;

  switch (archive_format)
    {
    case V7_FORMAT:
      break;

    case OLDGNU_FORMAT:
      /* Overwrite header->header.magic and header.version in one blow.  */
      strcpy (header->header.magic, OLDGNU_MAGIC);
      break;

    case POSIX_FORMAT:
    case USTAR_FORMAT:
    case GNU_FORMAT:   /*FIXME?*/
      strncpy (header->header.magic, TMAGIC, TMAGLEN);
      strncpy (header->header.version, TVERSION, TVERSLEN);
      break;

    default:
      abort ();
    }

  if (archive_format == V7_FORMAT || numeric_owner_option)
    {
      /* header->header.[ug]name are left as the empty string.  */
    }
  else
    {
      uid_to_uname (st->stat.st_uid, &st->uname);
      gid_to_gname (st->stat.st_gid, &st->gname);
      
      if (archive_format == POSIX_FORMAT
	  && strlen (st->uname) > UNAME_FIELD_SIZE)
	xheader_store ("uname", st);
      else
	UNAME_TO_CHARS (st->uname, header->header.uname);

      if (archive_format == POSIX_FORMAT
	  && strlen (st->gname) > GNAME_FIELD_SIZE)
	xheader_store ("gname", st);
      else
	GNAME_TO_CHARS (st->gname, header->header.gname);
    }

  return header;
}

/* Finish off a filled-in header block and write it out.  We also
   print the file name and/or full info if verbose is on.  If BLOCK_ORDINAL
   is not negative, is the block ordinal of the first record for this
   file, which may be a preceding long name or long link record.  */
void
finish_header (union block *header, off_t block_ordinal)
{
  size_t i;
  int sum;
  char *p;

  /* Note: It is important to do this before the call to write_extended(),
     so that the actual ustar header is printed */
  if (verbose_option
      && header->header.typeflag != GNUTYPE_LONGLINK
      && header->header.typeflag != GNUTYPE_LONGNAME
      && header->header.typeflag != XHDTYPE
      && header->header.typeflag != XGLTYPE)
    {
      /* These globals are parameters to print_header, sigh.  */

      current_header = header;
      /* current_stat_info is already set up.  */
      current_format = archive_format;
      print_header (block_ordinal);
    }

  header = write_extended (header, XHDTYPE);
  
  memcpy (header->header.chksum, CHKBLANKS, sizeof header->header.chksum);

  sum = 0;
  p = header->buffer;
  for (i = sizeof *header; i-- != 0; )
    /* We can't use unsigned char here because of old compilers, e.g. V7.  */
    sum += 0xFF & *p++;

  /* Fill in the checksum field.  It's formatted differently from the
     other fields: it has [6] digits, a null, then a space -- rather than
     digits, then a null.  We use to_chars.
     The final space is already there, from
     checksumming, and to_chars doesn't modify it.

     This is a fast way to do:

     sprintf(header->header.chksum, "%6o", sum);  */

  uintmax_to_chars ((uintmax_t) sum, header->header.chksum, 7);

  set_next_block_after (header);
}

/* Sparse file processing.  */

/* Takes a blockful of data and basically cruises through it to see if
   it's made *entirely* of zeros, returning a 0 the instant it finds
   something that is a nonzero, i.e., useful data.  */
static int
zero_block_p (char *buffer)
{
  int counter;

  for (counter = 0; counter < BLOCKSIZE; counter++)
    if (buffer[counter] != '\0')
      return 0;
  return 1;
}

void
init_sparsearray (void)
{
  if (! sp_array_size)
    sp_array_size = SPARSES_IN_OLDGNU_HEADER;
  sparsearray = xmalloc (sp_array_size * sizeof *sparsearray);
}

static off_t
find_new_file_size (int sparses)
{
  int i;
  off_t s = 0;
  for (i = 0; i < sparses; i++)
    s += sparsearray[i].numbytes;
  return s;
}

/* Make one pass over the file NAME, studying where any non-zero data
   is, that is, how far into the file each instance of data is, and
   how many bytes are there.  Save this information in the
   sparsearray, which will later be translated into header
   information.  */

/* There is little point in trimming small amounts of null data at the head
   and tail of blocks, only avoid dumping full null blocks.  */

/* FIXME: this routine might accept bits of algorithmic cleanup, it is
   too kludgey for my taste...  */

static int
deal_with_sparse (char *name, union block *header)
{
  size_t numbytes = 0;
  off_t offset = 0;
  int file;
  int sparses = 0;
  ssize_t count;
  char buffer[BLOCKSIZE];

  if (archive_format == OLDGNU_FORMAT)
    header->oldgnu_header.isextended = 0;

  if (file = open (name, O_RDONLY), file < 0)
    /* This problem will be caught later on, so just return.  */
    return 0;

  init_sparsearray ();
  clear_buffer (buffer);

  for (;;)
    {
      /* Realloc the scratch area as necessary.  FIXME: should reallocate
	 only at beginning of a new instance of non-zero data.  */

      if (sp_array_size <= sparses)
	{
	  sparsearray =
	    xrealloc (sparsearray,
		      2 * sp_array_size * sizeof (struct sp_array));
	  sp_array_size *= 2;
	}
      
      count = safe_read (file, buffer, sizeof buffer);
      if (count <= 0)
	break;

      /* Process one block.  */

      if (count == sizeof buffer)

	if (zero_block_p (buffer))
	  {
	    if (numbytes)
	      {
		sparsearray[sparses++].numbytes = numbytes;
		numbytes = 0;
	      }
	  }
	else
	  {
	    if (!numbytes)
	      sparsearray[sparses].offset = offset;
	    numbytes += count;
	  }

      else

	/* Since count < sizeof buffer, we have the last bit of the file.  */

	if (!zero_block_p (buffer))
	  {
	    if (!numbytes)
	      sparsearray[sparses].offset = offset;
	    numbytes += count;
	  }
	else
	  /* The next two lines are suggested by Andreas Degert, who says
	     they are required for trailing full blocks to be written to the
	     archive, when all zeroed.  Yet, it seems to me that the case
	     does not apply.  Further, at restore time, the file is not as
	     sparse as it should.  So, some serious cleanup is *also* needed
	     in this area.  Just one more... :-(.  FIXME.  */
	  if (numbytes)
	    numbytes += count;

      /* Prepare for next block.  */

      offset += count;
      /* FIXME: do not clear unless necessary.  */
      clear_buffer (buffer);
    }

  if (numbytes)
    sparsearray[sparses++].numbytes = numbytes;
  else
    {
      sparsearray[sparses].offset = offset - 1;
      sparsearray[sparses++].numbytes = 1;
    }

  return close (file) == 0 && 0 <= count ? sparses : 0;
}

static int
finish_sparse_file (int file, off_t *sizeleft, off_t fullsize, char *name)
{
  union block *start;
  size_t bufsize;
  int sparses = 0;
  ssize_t count;

  while (*sizeleft > 0)
    {
      start = find_next_block ();
      memset (start->buffer, 0, BLOCKSIZE);
      bufsize = sparsearray[sparses].numbytes;
      if (! bufsize)
	abort ();

      if (lseek (file, sparsearray[sparses++].offset, SEEK_SET) < 0)
	{
	  (ignore_failed_read_option ? seek_warn_details : seek_error_details)
	    (name, sparsearray[sparses - 1].offset);
	  break;
	}

      /* If the number of bytes to be written here exceeds the size of
	 the temporary buffer, do it in steps.  */

      while (bufsize > BLOCKSIZE)
	{
	  count = safe_read (file, start->buffer, BLOCKSIZE);
	  if (count < 0)
	    {
	      (ignore_failed_read_option
	       ? read_warn_details
	       : read_error_details)
		(name, fullsize - *sizeleft, bufsize);
	      return 1;
	    }
	  bufsize -= count;
	  *sizeleft -= count;
	  set_next_block_after (start);
	  start = find_next_block ();
	  memset (start->buffer, 0, BLOCKSIZE);
	}

      {
	char buffer[BLOCKSIZE];

	clear_buffer (buffer);
	count = safe_read (file, buffer, bufsize);
	memcpy (start->buffer, buffer, BLOCKSIZE);
      }

      if (count < 0)
	{
	  (ignore_failed_read_option
	   ? read_warn_details
	   : read_error_details)
	    (name, fullsize - *sizeleft, bufsize);
	  return 1;
	}

      *sizeleft -= count;
      set_next_block_after (start);
    }
  free (sparsearray);
#if 0
  set_next_block_after (start + (count - 1) / BLOCKSIZE);
#endif
  return 0;
}

/* Main functions of this module.  */

void
create_archive (void)
{
  char *p;

  open_archive (ACCESS_WRITE);

  if (incremental_option)
    {
      size_t buffer_size = 1000;
      char *buffer = xmalloc (buffer_size);
      const char *q;

      collect_and_sort_names ();

      while (p = name_from_list (), p)
	if (!excluded_name (p))
	  dump_file (p, -1, (dev_t) 0);

      blank_name_list ();
      while (p = name_from_list (), p)
	if (!excluded_name (p))
	  {
	    size_t plen = strlen (p);
	    if (buffer_size <= plen)
	      {
		while ((buffer_size *= 2) <= plen)
		  continue;
		buffer = xrealloc (buffer, buffer_size);
	      }
	    memcpy (buffer, p, plen);
	    if (! ISSLASH (buffer[plen - 1]))
	      buffer[plen++] = '/';
	    q = gnu_list_name->dir_contents;
	    if (q)
	      while (*q)
		{
		  size_t qlen = strlen (q);
		  if (*q == 'Y')
		    {
		      if (buffer_size < plen + qlen)
			{
			  while ((buffer_size *=2 ) < plen + qlen)
			    continue;
			  buffer = xrealloc (buffer, buffer_size);
			}
		      strcpy (buffer + plen, q + 1);
		      dump_file (buffer, -1, (dev_t) 0);
		    }
		  q += qlen + 1;
		}
	  }
      free (buffer);
    }
  else
    {
      while (p = name_next (1), p)
	if (!excluded_name (p))
	  dump_file (p, 1, (dev_t) 0);
    }

  write_eot ();
  close_archive ();

  if (listed_incremental_option)
    write_directory_file ();
}


/* Calculate the hash of a link.  */
static unsigned
hash_link (void const *entry, unsigned n_buckets)
{
  struct link const *link = entry;
  return (uintmax_t) (link->dev ^ link->ino) % n_buckets;
}

/* Compare two links for equality.  */
static bool
compare_links (void const *entry1, void const *entry2)
{
  struct link const *link1 = entry1;
  struct link const *link2 = entry2;
  return ((link1->dev ^ link2->dev) | (link1->ino ^ link2->ino)) == 0;
}

/* Table of all non-directories that we've written so far.  Any time
   we see another, we check the table and avoid dumping the data
   again if we've done it once already.  */
static Hash_table *link_table;

/* Dump a single file, recursing on directories.  P is the file name
   to dump.  TOP_LEVEL tells whether this is a top-level call; zero
   means no, positive means yes, and negative means the top level
   of an incremental dump.  PARENT_DEVICE is the device of P's
   parent directory; it is examined only if TOP_LEVEL is zero.

   Set global CURRENT_STAT_INFO to stat output for this file.  */

/* FIXME: One should make sure that for *every* path leading to setting
   exit_status to failure, a clear diagnostic has been issued.  */

void
dump_file (char *p, int top_level, dev_t parent_device)
{
  union block *header;
  char type;
  union block *exhdr;
  char save_typeflag;
  time_t original_ctime;
  struct utimbuf restore_times;
  off_t block_ordinal = -1;

  /* FIXME: `header' might be used uninitialized in this
     function.  Reported by Bruno Haible.  */

  if (interactive_option && !confirm ("add", p))
    return;

  if (deref_stat (dereference_option, p, &current_stat_info.stat) != 0)
    {
      if (ignore_failed_read_option)
	stat_warn (p);
      else
	stat_error (p);
      return;
    }

  original_ctime = current_stat_info.stat.st_ctime;
  restore_times.actime = current_stat_info.stat.st_atime;
  restore_times.modtime = current_stat_info.stat.st_mtime;

#ifdef S_ISHIDDEN
  if (S_ISHIDDEN (current_stat_info.stat.st_mode))
    {
      char *new = (char *) alloca (strlen (p) + 2);
      if (new)
	{
	  strcpy (new, p);
	  strcat (new, "@");
	  p = new;
	}
    }
#endif

  /* See if we want only new files, and check if this one is too old to
     put in the archive.  */

  if ((0 < top_level || !incremental_option)
      && !S_ISDIR (current_stat_info.stat.st_mode)
      && current_stat_info.stat.st_mtime < newer_mtime_option
      && (!after_date_option || current_stat_info.stat.st_ctime < newer_ctime_option))
    {
      if (0 < top_level)
	WARN ((0, 0, _("%s: file is unchanged; not dumped"),
	       quotearg_colon (p)));
      /* FIXME: recheck this return.  */
      return;
    }

  /* See if we are trying to dump the archive.  */
  if (sys_file_is_archive (&current_stat_info))
    {
      WARN ((0, 0, _("%s: file is the archive; not dumped"),
	     quotearg_colon (p)));
      return;
    }

  if (S_ISDIR (current_stat_info.stat.st_mode))
    {
      char *directory;
      char const *entry;
      size_t entrylen;
      char *namebuf;
      size_t buflen;
      size_t len;
      dev_t our_device = current_stat_info.stat.st_dev;

      errno = 0;

      directory = savedir (p);
      if (! directory)
	{
	  if (ignore_failed_read_option)
	    savedir_warn (p);
	  else
	    savedir_error (p);
	  return;
	}

      /* Build new prototype name.  Ensure exactly one trailing slash.  */

      len = strlen (p);
      buflen = len + NAME_FIELD_SIZE;
      namebuf = xmalloc (buflen + 1);
      memcpy (namebuf, p, len);
      while (len >= 1 && ISSLASH (namebuf[len - 1]))
	len--;
      namebuf[len++] = '/';
      namebuf[len] = '\0';

      if (! is_avoided_name (namebuf))
	{
	  /* The condition above used to be "archive_format != V7_FORMAT".
	     GNU tar was not writing directory blocks at all.  Daniel Trinkle
	     writes: ``All old versions of tar I have ever seen have
	     correctly archived an empty directory.  The really old ones I
	     checked included HP-UX 7 and Mt. Xinu More/BSD.  There may be
	     some subtle reason for the exclusion that I don't know, but the
	     current behavior is broken.''  I do not know those subtle
	     reasons either, so until these are reported (anew?), just allow
	     directory blocks to be written even with old archives.  */

	  block_ordinal = current_block_ordinal ();
	  current_stat_info.stat.st_size = 0;	/* force 0 size on dir */

	  header = start_header (namebuf, &current_stat_info);
	  if (!header)
	    return;
	  
	  if (incremental_option)
	    header->header.typeflag = GNUTYPE_DUMPDIR;
	  else /* if (standard_option) */
	    header->header.typeflag = DIRTYPE;

	  /* If we're gnudumping, we aren't done yet so don't close it.  */

	  if (!incremental_option)
	    finish_header (header, block_ordinal);
	}

      if (incremental_option && gnu_list_name->dir_contents)
	{
	  off_t sizeleft;
	  off_t totsize;
	  size_t bufsize;
	  union block *start;
	  ssize_t count;
	  const char *buffer, *p_buffer;

	  buffer = gnu_list_name->dir_contents; /* FOO */
	  totsize = 0;
	  if (buffer)
	    for (p_buffer = buffer; *p_buffer; )
	      {
		size_t size = strlen (p_buffer) + 1;
		totsize += size;
		p_buffer += size;
	      }
	  totsize++;
	  OFF_TO_CHARS (totsize, header->header.size);
	  finish_header (header, block_ordinal);
	  p_buffer = buffer;
	  sizeleft = totsize;
	  while (sizeleft > 0)
	    {
	      if (multi_volume_option)
		{
		  assign_string (&save_name, p);
		  save_sizeleft = sizeleft;
		  save_totsize = totsize;
		}
	      start = find_next_block ();
	      bufsize = available_space_after (start);
	      if (sizeleft < bufsize)
		{
		  bufsize = sizeleft;
		  count = bufsize % BLOCKSIZE;
		  if (count)
		    memset (start->buffer + sizeleft, 0, BLOCKSIZE - count);
		}
	      memcpy (start->buffer, p_buffer, bufsize);
	      sizeleft -= bufsize;
	      p_buffer += bufsize;
	      set_next_block_after (start + (bufsize - 1) / BLOCKSIZE);
	    }
	  if (multi_volume_option)
	    assign_string (&save_name, 0);
	  goto finish_dir;
	}

      /* See if we are about to recurse into a directory, and avoid doing
	 so if the user wants that we do not descend into directories.  */

      if (! recursion_option)
	goto finish_dir;

      /* See if we are crossing from one file system to another, and
	 avoid doing so if the user only wants to dump one file system.  */

      if (one_file_system_option && !top_level
	  && parent_device != current_stat_info.stat.st_dev)
	{
	  if (verbose_option)
	    WARN ((0, 0,
		   _("%s: file is on a different filesystem; not dumped"),
		   quotearg_colon (p)));
	  goto finish_dir;
	}

      /* Now output all the files in the directory.  */

      /* FIXME: Should speed this up by cd-ing into the dir.  */

      for (entry = directory;
	   (entrylen = strlen (entry)) != 0;
	   entry += entrylen + 1)
	{
	  if (buflen < len + entrylen)
	    {
	      buflen = len + entrylen;
	      namebuf = xrealloc (namebuf, buflen + 1);
	    }
	  strcpy (namebuf + len, entry);
	  if (!excluded_name (namebuf))
	    dump_file (namebuf, 0, our_device);
	}

    finish_dir:

      free (directory);
      free (namebuf);
      if (atime_preserve_option)
	utime (p, &restore_times);
      return;
    }
  else if (is_avoided_name (p))
    return;
  else
    {
      /* Check for multiple links.  */

      if (1 < current_stat_info.stat.st_nlink && link_table)
	{
	  struct link lp;
	  struct link *dup;
	  lp.ino = current_stat_info.stat.st_ino;
	  lp.dev = current_stat_info.stat.st_dev;

	  if ((dup = hash_lookup (link_table, &lp)))
	    {
	      /* We found a link.  */
	      char const *link_name = safer_name_suffix (dup->name, 1);

	      dup->nlink--;
	      
	      block_ordinal = current_block_ordinal ();
	      assign_string (&current_stat_info.link_name, link_name);
	      if (NAME_FIELD_SIZE < strlen (link_name))
		write_long_link (&current_stat_info);

	      current_stat_info.stat.st_size = 0;
	      header = start_header (p, &current_stat_info);
	      if (!header)
		return;
	      tar_copy_str (header->header.linkname, link_name,
			    NAME_FIELD_SIZE);

	      header->header.typeflag = LNKTYPE;
	      finish_header (header, block_ordinal);

	      /* FIXME: Maybe remove from table after all links found?  */

	      if (remove_files_option && unlink (p) != 0)
		unlink_error (p);

	      /* We dumped it, and we don't need to put it in the
                 table again.  */
	      return;
	    }
	}

      /* This is not a link to a previously dumped file, so dump it.  */

      if (S_ISREG (current_stat_info.stat.st_mode)
	  || S_ISCTG (current_stat_info.stat.st_mode))
	{
	  int f;			/* file descriptor */
	  size_t bufsize;
	  ssize_t count;
	  off_t sizeleft;
	  union block *start;
	  int header_moved;
	  char isextended = 0;
	  int sparses = 0;

	  header_moved = 0;

	  if (sparse_option)
	    {
	      /* Check the size of the file against the number of blocks
		 allocated for it, counting both data and indirect blocks.
		 If there is a smaller number of blocks than would be
		 necessary to accommodate a file of this size, this is safe
		 to say that we have a sparse file: at least one of those
		 blocks in the file is just a useless hole.  For sparse
		 files not having more hole blocks than indirect blocks, the
		 sparseness will go undetected.  */

	      /* Bruno Haible sent me these statistics for Linux.  It seems
		 that some filesystems count indirect blocks in st_blocks,
		 while others do not seem to:

		 minix-fs   tar: size=7205, st_blocks=18 and ST_NBLOCKS=18
		 extfs      tar: size=7205, st_blocks=18 and ST_NBLOCKS=18
		 ext2fs     tar: size=7205, st_blocks=16 and ST_NBLOCKS=16
		 msdos-fs   tar: size=7205, st_blocks=16 and ST_NBLOCKS=16

		 Dick Streefland reports the previous numbers as misleading,
		 because ext2fs use 12 direct blocks, while minix-fs uses only
		 6 direct blocks.  Dick gets:

		 ext2	size=20480	ls listed blocks=21
		 minix	size=20480	ls listed blocks=21
		 msdos	size=20480	ls listed blocks=20

		 It seems that indirect blocks *are* included in st_blocks.
		 The minix filesystem does not account for phantom blocks in
		 st_blocks, so `du' and `ls -s' give wrong results.  So, the
		 --sparse option would not work on a minix filesystem.  */

	      if (ST_NBLOCKS (current_stat_info.stat)
		  < (current_stat_info.stat.st_size / ST_NBLOCKSIZE
		     + (current_stat_info.stat.st_size % ST_NBLOCKSIZE != 0)))
		{
		  int counter;

		  block_ordinal = current_block_ordinal ();
		  header = start_header (p, &current_stat_info);
		  if (!header)
		    return;
		  header->header.typeflag = GNUTYPE_SPARSE;
		  header_moved = 1;

		  /* Call the routine that figures out the layout of the
		     sparse file in question.  SPARSES is the index of the
		     first unused element of the "sparsearray," i.e.,
		     the number of elements it needed to describe the file.  */

		  sparses = deal_with_sparse (p, header);

		  /* See if we'll need an extended header later.  */

		  if (SPARSES_IN_OLDGNU_HEADER < sparses)
		    header->oldgnu_header.isextended = 1;

		  /* We store the "real" file size so we can show that in
		     case someone wants to list the archive, i.e., tar tvf
		     <file>.  It might be kind of disconcerting if the
		     shrunken file size was the one that showed up.  */

		  OFF_TO_CHARS (current_stat_info.stat.st_size,
				header->oldgnu_header.realsize);

		  /* This will be the new "size" of the file, i.e., the size
		     of the file minus the blocks of holes that we're
		     skipping over.  */

		  current_stat_info.stat.st_size = find_new_file_size (sparses);
		  OFF_TO_CHARS (current_stat_info.stat.st_size, header->header.size);

		  for (counter = 0;
		       counter < sparses && counter < SPARSES_IN_OLDGNU_HEADER;
		       counter++)
		    {
		      OFF_TO_CHARS (sparsearray[counter].offset,
				    header->oldgnu_header.sp[counter].offset);
		      SIZE_TO_CHARS (sparsearray[counter].numbytes,
				     header->oldgnu_header.sp[counter].numbytes);
		    }
		}
	    }

	  sizeleft = current_stat_info.stat.st_size;

	  /* Don't bother opening empty, world readable files.  Also do not
	     open files when archive is meant for /dev/null.  */

	  if (dev_null_output
	      || (sizeleft == 0
		  && MODE_R == (MODE_R & current_stat_info.stat.st_mode)))
	    f = -1;
	  else
	    {
	      f = open (p, O_RDONLY | O_BINARY);
	      if (f < 0)
		{
		  if (! top_level && errno == ENOENT)
		    WARN ((0, 0, _("%s: File removed before we read it"),
			   quotearg_colon (p)));
		  else
		    (ignore_failed_read_option ? open_warn : open_error) (p);
		  return;
		}
	    }

	  /* If the file is sparse, we've already taken care of this.  */

	  if (!header_moved)
	    {
	      block_ordinal = current_block_ordinal ();
	      header = start_header (p, &current_stat_info);
	      if (!header)
		return;
	    }

	  /* Mark contiguous files, if we support them.  */

	  if (archive_format != V7_FORMAT
	      && S_ISCTG (current_stat_info.stat.st_mode))
	    header->header.typeflag = CONTTYPE;

	  if (archive_format == GNU_FORMAT || archive_format == OLDGNU_FORMAT)
	    isextended = header->oldgnu_header.isextended;
	  else
	    isextended = 0;

	  save_typeflag = header->header.typeflag;
	  finish_header (header, block_ordinal);
	  if (isextended)
	    {
	      int sparses_emitted = SPARSES_IN_OLDGNU_HEADER;

	      for (;;)
		{
		  int i;
		  exhdr = find_next_block ();
		  memset (exhdr->buffer, 0, BLOCKSIZE);
		  for (i = 0;
		       (i < SPARSES_IN_SPARSE_HEADER
			&& sparses_emitted + i < sparses);
		       i++)
		    {
		      SIZE_TO_CHARS (sparsearray[sparses_emitted + i].numbytes,
				     exhdr->sparse_header.sp[i].numbytes);
		      OFF_TO_CHARS (sparsearray[sparses_emitted + i].offset,
				    exhdr->sparse_header.sp[i].offset);
		    }
		  set_next_block_after (exhdr);
		  sparses_emitted += i;
		  if (sparses == sparses_emitted)
		    break;
		  exhdr->sparse_header.isextended = 1;
		}
	    }
	  if (save_typeflag == GNUTYPE_SPARSE)
	    {
	      if (f < 0
		  || finish_sparse_file (f, &sizeleft,
					 current_stat_info.stat.st_size, p))
		goto padit;
	    }
	  else
	    while (sizeleft > 0)
	      {
		if (multi_volume_option)
		  {
		    assign_string (&save_name, p);
		    save_sizeleft = sizeleft;
		    save_totsize = current_stat_info.stat.st_size;
		  }
		start = find_next_block ();

		bufsize = available_space_after (start);

		if (sizeleft < bufsize)
		  {
		    /* Last read -- zero out area beyond.  */

		    bufsize = sizeleft;
		    count = bufsize % BLOCKSIZE;
		    if (count)
		      memset (start->buffer + sizeleft, 0, BLOCKSIZE - count);
		  }
		if (f < 0)
		  count = bufsize;
		else
		  count = safe_read (f, start->buffer, bufsize);
		if (count < 0)
		  {
		    (ignore_failed_read_option
		     ? read_warn_details
		     : read_error_details)
		      (p, current_stat_info.stat.st_size - sizeleft, bufsize);
		    goto padit;
		  }
		sizeleft -= count;

		/* This is nonportable (the type of set_next_block_after's arg).  */

		set_next_block_after (start + (bufsize - 1) / BLOCKSIZE);


		if (count != bufsize)
		  {
		    char buf[UINTMAX_STRSIZE_BOUND];
		    memset (start->buffer + count, 0, bufsize - count);
		    WARN ((0, 0,
			   ngettext ("%s: File shrank by %s byte; padding with zeros",
				     "%s: File shrank by %s bytes; padding with zeros",
				     sizeleft),
			   quotearg_colon (p),
			   STRINGIFY_BIGINT (sizeleft, buf)));
		    if (! ignore_failed_read_option)
		      exit_status = TAREXIT_FAILURE;
		    goto padit;		/* short read */
		  }
	      }

	  if (multi_volume_option)
	    assign_string (&save_name, 0);

	  if (f >= 0)
	    {
	      struct stat final_stat;
	      if (fstat (f, &final_stat) != 0)
		{
		  if (ignore_failed_read_option)
		    stat_warn (p);
		  else
		    stat_error (p);
		}
	      else if (final_stat.st_ctime != original_ctime)
		{
		  char const *qp = quotearg_colon (p);
		  WARN ((0, 0, _("%s: file changed as we read it"), qp));
		}
	      if (close (f) != 0)
		{
		  if (ignore_failed_read_option)
		    close_warn (p);
		  else
		    close_error (p);
		}
	      if (atime_preserve_option)
		utime (p, &restore_times);
	    }
	  if (remove_files_option)
	    {
	      if (unlink (p) == -1)
		unlink_error (p);
	    }
	  goto file_was_dumped;

	  /* File shrunk or gave error, pad out tape to match the size we
	     specified in the header.  */

	padit:
	  while (sizeleft > 0)
	    {
	      save_sizeleft = sizeleft;
	      start = find_next_block ();
	      memset (start->buffer, 0, BLOCKSIZE);
	      set_next_block_after (start);
	      sizeleft -= BLOCKSIZE;
	    }
	  if (multi_volume_option)
	    assign_string (&save_name, 0);
	  if (f >= 0)
	    {
	      close (f);
	      if (atime_preserve_option)
		utime (p, &restore_times);
	    }
	  goto file_was_dumped;
	}
#ifdef HAVE_READLINK
      else if (S_ISLNK (current_stat_info.stat.st_mode))
	{
	  char *buffer;
	  int size;
	  size_t linklen = current_stat_info.stat.st_size;
	  if (linklen != current_stat_info.stat.st_size || linklen + 1 == 0)
	    xalloc_die ();
	  buffer = (char *) alloca (linklen + 1);
	  size = readlink (p, buffer, linklen + 1);
	  if (size < 0)
	    {
	      if (ignore_failed_read_option)
		readlink_warn (p);
	      else
		readlink_error (p);
	      return;
	    }
	  buffer[size] = '\0';
	  assign_string (&current_stat_info.link_name, buffer);
	  if (size > NAME_FIELD_SIZE)
	    write_long_link (&current_stat_info);

	  block_ordinal = current_block_ordinal ();
	  current_stat_info.stat.st_size = 0;	/* force 0 size on symlink */
	  header = start_header (p, &current_stat_info);
	  if (!header)
	    return;
	  tar_copy_str (header->header.linkname, buffer, NAME_FIELD_SIZE);
	  header->header.typeflag = SYMTYPE;
	  finish_header (header, block_ordinal);
	  /* nothing more to do to it */

	  if (remove_files_option)
	    {
	      if (unlink (p) == -1)
		unlink_error (p);
	    }
	  goto file_was_dumped;
	}
#endif
      else if (S_ISCHR (current_stat_info.stat.st_mode))
	type = CHRTYPE;
      else if (S_ISBLK (current_stat_info.stat.st_mode))
	type = BLKTYPE;
      else if (S_ISFIFO (current_stat_info.stat.st_mode))
	type = FIFOTYPE;
      else if (S_ISSOCK (current_stat_info.stat.st_mode))
	{
	  WARN ((0, 0, _("%s: socket ignored"), quotearg_colon (p)));
	  return;
	}
      else if (S_ISDOOR (current_stat_info.stat.st_mode))
	{
	  WARN ((0, 0, _("%s: door ignored"), quotearg_colon (p)));
	  return;
	}
      else
	goto unknown;
    }

  if (archive_format == V7_FORMAT)
    goto unknown;

  block_ordinal = current_block_ordinal ();
  current_stat_info.stat.st_size = 0;	/* force 0 size */
  header = start_header (p, &current_stat_info);
  if (!header)
    return;
  header->header.typeflag = type;

  if (type != FIFOTYPE)
    {
      MAJOR_TO_CHARS (major (current_stat_info.stat.st_rdev),
		      header->header.devmajor);
      MINOR_TO_CHARS (minor (current_stat_info.stat.st_rdev),
		      header->header.devminor);
    }

  finish_header (header, block_ordinal);
  if (remove_files_option)
    {
      if (unlink (p) == -1)
	unlink_error (p);
    }
  goto file_was_dumped;

unknown:
  WARN ((0, 0, _("%s: Unknown file type; file ignored"),
	 quotearg_colon (p)));
  if (! ignore_failed_read_option)
    exit_status = TAREXIT_FAILURE;
  return;

file_was_dumped:
  if (1 < current_stat_info.stat.st_nlink)
    {
      struct link *dup;
      struct link *lp = xmalloc (offsetof (struct link, name)
				 + strlen (p) + 1);
      lp->ino = current_stat_info.stat.st_ino;
      lp->dev = current_stat_info.stat.st_dev;
      lp->nlink = current_stat_info.stat.st_nlink;
      strcpy (lp->name, p);

      if (! ((link_table
	      || (link_table = hash_initialize (0, 0, hash_link,
						compare_links, 0)))
	     && (dup = hash_insert (link_table, lp))))
	xalloc_die ();

      if (dup != lp)
	abort ();
      lp->nlink--;
    }

}

/* For each dumped file, check if all its links were dumped. Emit
   warnings if it is not so. */
void
check_links ()
{
  struct link *lp;

  if (!link_table)
    return;

  for (lp = hash_get_first (link_table); lp;
       lp = hash_get_next (link_table, lp))
    {
      if (lp->nlink)
	{
	  WARN ((0, 0, _("Missing links to '%s'.\n"), lp->name));
	}
    }
}

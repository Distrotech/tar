/* POSIX extended headers for tar.

   Copyright (C) 2003, 2004 Free Software Foundation, Inc.

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

#include <hash.h>
#include <quotearg.h>
#include <xstrtol.h>

#include "common.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
#include <obstack.h>

bool xheader_protected_pattern_p (const char *pattern);
bool xheader_protected_keyword_p (const char *keyword);

/* Number of the global headers written so far. Not used yet */
static size_t global_header_count;


/* Keyword options */

struct keyword_list
{
  struct keyword_list *next;
  char *pattern;
  char *value;
};


/* List of keyword patterns set by delete= option */
static struct keyword_list *keyword_pattern_list;
/* List of keyword/value pairs set by `keyword=value' option */
static struct keyword_list *keyword_global_override_list;
/* List of keyword/value pairs set by `keyword:=value' option */
static struct keyword_list *keyword_override_list;
/* Template for the name field of an 'x' type header */
static char *exthdr_name;
/* Template for the name field of a 'g' type header */
static char *globexthdr_name;

bool
xheader_keyword_deleted_p (const char *kw)
{
  struct keyword_list *kp;

  for (kp = keyword_pattern_list; kp; kp = kp->next)
    if (fnmatch (kp->pattern, kw, 0) == 0)
      return true;
  return false;
}

bool
xheader_keyword_override_p (const char *keyword)
{
  struct keyword_list *kp;

  for (kp = keyword_override_list; kp; kp = kp->next)
    if (strcmp (kp->pattern, keyword) == 0)
      return true;
  return false;
}

void
xheader_list_append (struct keyword_list **root, char *kw, char *value)
{
  struct keyword_list *kp = xmalloc (sizeof *kp);
  kp->pattern = xstrdup (kw);
  kp->value = value ? xstrdup (value) : NULL;
  kp->next = *root;
  *root = kp;
}

void
xheader_set_single_keyword (char *kw)
{
  USAGE_ERROR ((0, 0, "Keyword %s is unknown or not yet imlemented", kw));
}

void
xheader_set_keyword_equal (char *kw, char *eq)
{
  bool global = true;
  char *p = eq;

  if (eq[-1] == ':')
    {
      p--;
      global = false;
    }

  while (p > kw && isspace (*p))
    p--;

  *p = 0;

  for (p = eq + 1; *p && isspace (*p); p++)
    ;
  
  if (strcmp (kw, "delete") == 0)
    {
      if (xheader_protected_pattern_p (p))
	USAGE_ERROR ((0, 0, "Pattern %s cannot be used", p));
      xheader_list_append (&keyword_pattern_list, p, NULL);
    }
  else if (strcmp (kw, "exthdr.name") == 0)
    assign_string (&exthdr_name, p);
  else if (strcmp (kw, "globexthdr.name") == 0)
    assign_string (&globexthdr_name, p);
  else
    {
      if (xheader_protected_keyword_p (kw))
	USAGE_ERROR ((0, 0, "Keyword %s cannot be overridden", kw));
      if (global)
	xheader_list_append (&keyword_global_override_list, kw, p);
      else
	xheader_list_append (&keyword_override_list, kw, p);
    }
}

void
xheader_set_option (char *string)
{
  char *token;
  for (token = strtok (string, ","); token; token = strtok (NULL, ","))
    {
      char *p = strchr (token, '=');
      if (!p)
	xheader_set_single_keyword (token);
      else
	xheader_set_keyword_equal (token, p);
    }
}

/*
    string Includes:          Replaced By:
     %d                       The directory name of the file,
                              equivalent to the result of the
                              dirname utility on the translated
                              pathname.
     %f                       The filename of the file, equivalent
                              to the result of the basename
                              utility on the translated pathname.
     %p                       The process ID of the pax process.
     %%                       A '%' character. */

static char *
xheader_format_name (struct tar_stat_info *st, const char *fmt, bool allow_n)
{
  char *buf;
  size_t len = strlen (fmt);
  char *q;
  const char *p;
  char *dirname = NULL;
  char *basename = NULL;
  char pidbuf[64];
  char nbuf[64];
  
  for (p = exthdr_name; *p && (p = strchr (p, '%')); )
    {
      switch (p[1])
	{
	case '%':
	  len--;
	  break;

	case 'd':
	  dirname = safer_name_suffix (dir_name (st->orig_file_name), false);
	  len += strlen (dirname) - 1;
	  break;
	      
	case 'f':
	  basename = base_name (st->orig_file_name);
	  len += strlen (basename) - 1;
	  break;
	      
	case 'p':
	  snprintf (pidbuf, sizeof pidbuf, "%lu",
		    (unsigned long) getpid ());
	  len += strlen (pidbuf) - 1;
	  break;
	  
	case 'n':
	  if (allow_n)
	    {
	      snprintf (nbuf, sizeof nbuf, "%lu",
			(unsigned long) global_header_count + 1);
	      len += strlen (nbuf) - 1;
	    }
	  break;
	}
      p++;
    }
  
  buf = xmalloc (len + 1);
  for (q = buf, p = fmt; *p; )
    {
      if (*p == '%')
	{
	  switch (p[1])
	    {
	    case '%':
	      *q++ = *p++;
	      p++;
	      break;
	      
	    case 'd':
	      q = stpcpy (q, dirname);
	      p += 2;
	      break;
	      
	    case 'f':
	      q = stpcpy (q, basename);
	      p += 2;
	      break;
	      
	    case 'p':
	      q = stpcpy (q, pidbuf);
	      p += 2;
	      break;

	    case 'n':
	      if (allow_n)
		{
		  q = stpcpy (q, nbuf);
		  p += 2;
		}
	      /* else fall through */
	      
	    default:
	      *q++ = *p++;
	      if (*p)
		*q++ = *p++;
	    }
	}
      else
	*q++ = *p++;
    }

  /* Do not allow it to end in a slash */
  while (q > buf && ISSLASH (q[-1]))
    q--;
  *q = 0;
  return buf;
}

char *
xheader_xhdr_name (struct tar_stat_info *st)
{
  /* FIXME: POSIX requires the default name to be '%d/PaxHeaders.%p/%f' */
  if (!exthdr_name)
    return xstrdup ("././@PaxHeader");
  return xheader_format_name (st, exthdr_name, false);
}

#define GLOBAL_HEADER_TEMPLATE "/GlobalHead.%p.%n"

char *
xheader_ghdr_name (struct tar_stat_info *st)
{
  if (!globexthdr_name)
    {
      size_t len;
      const char *tmp = getenv ("TMPDIR");
      if (!tmp)
	tmp = "/tmp";
      len = strlen (tmp) + sizeof (GLOBAL_HEADER_TEMPLATE); /* Includes nul */
      globexthdr_name = xmalloc (len);
      strcpy(globexthdr_name, tmp);
      strcat(globexthdr_name, GLOBAL_HEADER_TEMPLATE);
    }

  return xheader_format_name (st, globexthdr_name, true);
}


/* General Interface */

/* Used by xheader_finish() */
static void code_string (char const *string, char const *keyword,
			 struct xheader *xhdr);

struct xhdr_tab
{
  char const *keyword;
  void (*coder) (struct tar_stat_info const *, char const *,
		 struct xheader *, void *data);
  void (*decoder) (struct tar_stat_info *, char const *);
  bool protect;
};

/* This declaration must be extern, because ISO C99 section 6.9.2
   prohibits a tentative definition that has both internal linkage and
   incomplete type.  If we made it static, we'd have to declare its
   size which would be a maintenance pain; if we put its initializer
   here, we'd need a boatload of forward declarations, which would be
   even more of a pain.  */
extern struct xhdr_tab const xhdr_tab[];

static struct xhdr_tab const *
locate_handler (char const *keyword)
{
  struct xhdr_tab const *p;

  for (p = xhdr_tab; p->keyword; p++)
    if (strcmp (p->keyword, keyword) == 0)
      return p;
  return NULL;
}

bool
xheader_protected_pattern_p (const char *pattern)
{
  struct xhdr_tab const *p;

  for (p = xhdr_tab; p->keyword; p++)
    if (p->protect && fnmatch (pattern, p->keyword, 0) == 0)
      return true;
  return false;
}

bool
xheader_protected_keyword_p (const char *keyword)
{
  struct xhdr_tab const *p;

  for (p = xhdr_tab; p->keyword; p++)
    if (p->protect && strcmp (p->keyword, keyword) == 0)
      return true;
  return false;
}

/* Decodes a single extended header record. Advances P to the next
   record.
   Returns true on success, false otherwise. */
static bool
decode_record (char **p, struct tar_stat_info *st)
{
  size_t len;
  char const *keyword;
  char *eqp;
  char *start = *p;
  struct xhdr_tab const *t;

  if (**p == 0)
    return false;

  len = strtoul (*p, p, 10);
  if (**p != ' ')
    {
      ERROR ((0, 0,
       _("Malformed extended header: missing whitespace after the length")));
      return false;
    }

  keyword = ++*p;
  for (;*p < start + len; ++*p)
    if (**p == '=')
      break;

  if (**p != '=')
    {
      ERROR ((0, 0, _("Malformed extended header: missing equal sign")));
      return false;
    }

  eqp = *p;
  **p = 0;

  if (xheader_keyword_deleted_p (keyword)
      || xheader_keyword_override_p (keyword))
    return true;
  t = locate_handler (keyword);
  if (t)
    {
      char endc;
      char *value;

      value = ++*p;

      endc = start[len-1];
      start[len-1] = 0;
      t->decoder (st, value);
      start[len-1] = endc;
    }
  *eqp = '=';
  *p = &start[len];
  return true;
}

static void
run_override_list (struct keyword_list *kp, struct tar_stat_info *st)
{
  for (; kp; kp = kp->next)
    {
      struct xhdr_tab const *t = locate_handler (kp->pattern);
      if (t)
	t->decoder (st, kp->value);
    }
}

void
xheader_decode (struct tar_stat_info *st)
{
  char *p = extended_header.buffer + BLOCKSIZE;
  char *endp = &extended_header.buffer[extended_header.size-1];

  run_override_list (keyword_global_override_list, st);
  
  while (p < endp)
    if (!decode_record (&p, st))
      break;

  run_override_list (keyword_override_list, st);
}

void
xheader_store (char const *keyword, struct tar_stat_info const *st, void *data)
{
  struct xhdr_tab const *t;
  char *value;
  
  if (extended_header.buffer)
    return;
  t = locate_handler (keyword);
  if (!t)
    return;
  if (xheader_keyword_deleted_p (keyword)
      || xheader_keyword_override_p (keyword))
    return;
  if (!extended_header.stk)
    {
      extended_header.stk = xmalloc (sizeof *extended_header.stk);
      obstack_init (extended_header.stk);
    }
  t->coder (st, keyword, &extended_header, data);
}

void
xheader_read (union block *p, size_t size)
{
  size_t j = 0;
  size_t nblocks;

  free (extended_header.buffer);
  size += BLOCKSIZE;
  extended_header.size = size;
  nblocks = (size + BLOCKSIZE - 1) / BLOCKSIZE;
  extended_header.buffer = xmalloc (size + 1);

  do
    {
      size_t len = size;

      if (len > BLOCKSIZE)
	len = BLOCKSIZE;
      
      memcpy (&extended_header.buffer[j], p->buffer, len);
      set_next_block_after (p);

      p = find_next_block ();

      j += len;
      size -= len;
    }
  while (size > 0);
}

static size_t
format_uintmax (uintmax_t val, char *buf, size_t s)
{
  if (!buf)
    {
      s = 0;
      do
	s++;
      while ((val /= 10) != 0);
    }
  else
    {
      char *p = buf + s - 1;

      do
	{
	  *p-- = val % 10 + '0';
	}
      while ((val /= 10) != 0);

      while (p >= buf)
	*p-- = '0';
    }
  return s;
}

static void
xheader_print (struct xheader *xhdr, char const *keyword, char const *value)
{
  size_t len = strlen (keyword) + strlen (value) + 3; /* ' ' + '=' + '\n' */
  size_t p, n = 0;
  char nbuf[100];

  do
    {
      p = n;
      n = format_uintmax (len + p, NULL, 0);
    }
  while (n != p);

  format_uintmax (len + n, nbuf, n);
  obstack_grow (xhdr->stk, nbuf, n);
  obstack_1grow (xhdr->stk, ' ');
  obstack_grow (xhdr->stk, keyword, strlen (keyword));
  obstack_1grow (xhdr->stk, '=');
  obstack_grow (xhdr->stk, value, strlen (value));
  obstack_1grow (xhdr->stk, '\n');
}

void
xheader_finish (struct xheader *xhdr)
{
  struct keyword_list *kp;

  for (kp = keyword_override_list; kp; kp = kp->next)
    code_string (kp->value, kp->pattern, xhdr);

  obstack_1grow (xhdr->stk, 0);
  xhdr->buffer = obstack_finish (xhdr->stk);
  xhdr->size = strlen (xhdr->buffer);
}

void
xheader_destroy (struct xheader *xhdr)
{
  if (xhdr->stk)
    {
      obstack_free (xhdr->stk, NULL);
      free (xhdr->stk);
      xhdr->stk = NULL;
    }
  else
    free (xhdr->buffer);
  xhdr->buffer = 0;
  xhdr->size = 0;
}


/* Implementations */
static void
code_string (char const *string, char const *keyword, struct xheader *xhdr)
{
  xheader_print (xhdr, keyword, string);
}

static void
code_time (time_t t, unsigned long nano,
	   char const *keyword, struct xheader *xhdr)
{
  char sbuf[200];
  size_t s = format_uintmax (t, NULL, 0);
  if (s + 11 >= sizeof sbuf)
    return;
  format_uintmax (t, sbuf, s);
  sbuf[s++] = '.';
  s += format_uintmax (nano, sbuf + s, 9);
  sbuf[s] = 0;
  xheader_print (xhdr, keyword, sbuf);
}

static void
decode_time (char const *arg, time_t *secs, unsigned long *nsecs)
{
  uintmax_t u;
  char *p;
  if (xstrtoumax (arg, &p, 10, &u, "") == LONGINT_OK)
    {
      *secs = u;
      if (*p == '.' && xstrtoumax (p+1, NULL, 10, &u, "") == LONGINT_OK)
	*nsecs = u;
    }
}

static void
code_num (uintmax_t value, char const *keyword, struct xheader *xhdr)
{
  char sbuf[100];
  size_t s = format_uintmax (value, NULL, 0);
  format_uintmax (value, sbuf, s);
  sbuf[s] = 0;
  xheader_print (xhdr, keyword, sbuf);
}

static void
dummy_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
}

static void
dummy_decoder (struct tar_stat_info *st, char const *arg)
{
}

static void
atime_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  code_time (st->stat.st_atime, st->atime_nsec, keyword, xhdr);
}

static void
atime_decoder (struct tar_stat_info *st, char const *arg)
{
  decode_time (arg, &st->stat.st_atime, &st->atime_nsec);
}

static void
gid_coder (struct tar_stat_info const *st, char const *keyword,
	   struct xheader *xhdr, void *data)
{
  code_num (st->stat.st_gid, keyword, xhdr);
}

static void
gid_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    st->stat.st_gid = u;
}

static void
gname_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  code_string (st->gname, keyword, xhdr);
}

static void
gname_decoder (struct tar_stat_info *st, char const *arg)
{
  assign_string (&st->gname, arg);
}

static void
linkpath_coder (struct tar_stat_info const *st, char const *keyword,
		struct xheader *xhdr, void *data)
{
  code_string (st->link_name, keyword, xhdr);
}

static void
linkpath_decoder (struct tar_stat_info *st, char const *arg)
{
  assign_string (&st->link_name, arg);
}

static void
ctime_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  code_time (st->stat.st_ctime, st->ctime_nsec, keyword, xhdr);
}

static void
ctime_decoder (struct tar_stat_info *st, char const *arg)
{
  decode_time (arg, &st->stat.st_ctime, &st->ctime_nsec);
}

static void
mtime_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  code_time (st->stat.st_mtime, st->mtime_nsec, keyword, xhdr);
}

static void
mtime_decoder (struct tar_stat_info *st, char const *arg)
{
  decode_time (arg, &st->stat.st_mtime, &st->mtime_nsec);
}

static void
path_coder (struct tar_stat_info const *st, char const *keyword,
	    struct xheader *xhdr, void *data)
{
  code_string (st->file_name, keyword, xhdr);
}

static void
path_decoder (struct tar_stat_info *st, char const *arg)
{
  assign_string (&st->orig_file_name, arg);
  assign_string (&st->file_name, arg);
  st->had_trailing_slash = strip_trailing_slashes (st->file_name);
}

static void
size_coder (struct tar_stat_info const *st, char const *keyword,
	    struct xheader *xhdr, void *data)
{
  code_num (st->stat.st_size, keyword, xhdr);
}

static void
size_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    st->stat.st_size = u;
}

static void
uid_coder (struct tar_stat_info const *st, char const *keyword,
	   struct xheader *xhdr, void *data)
{
  code_num (st->stat.st_uid, keyword, xhdr);
}

static void
uid_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    st->stat.st_uid = u;
}

static void
uname_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  code_string (st->uname, keyword, xhdr);
}

static void
uname_decoder (struct tar_stat_info *st, char const *arg)
{
  assign_string (&st->uname, arg);
}

static void
sparse_size_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  size_coder (st, keyword, xhdr, data);
}

static void
sparse_size_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    st->archive_file_size = u;
}

static void
sparse_numblocks_coder (struct tar_stat_info const *st, char const *keyword,
			struct xheader *xhdr, void *data)
{
  code_num (st->sparse_map_avail, keyword, xhdr);
}

static void
sparse_numblocks_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    {
      st->sparse_map_size = u;
      st->sparse_map = calloc(st->sparse_map_size, sizeof(st->sparse_map[0]));
      st->sparse_map_avail = 0;
    }
}

static void
sparse_offset_coder (struct tar_stat_info const *st, char const *keyword,
		     struct xheader *xhdr, void *data)
{
  size_t i = *(size_t*)data;
  code_num (st->sparse_map[i].offset, keyword, xhdr);
}

static void
sparse_offset_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    st->sparse_map[st->sparse_map_avail].offset = u;
}

static void
sparse_numbytes_coder (struct tar_stat_info const *st, char const *keyword,
		     struct xheader *xhdr, void *data)
{
  size_t i = *(size_t*)data;
  code_num (st->sparse_map[i].numbytes, keyword, xhdr);
}

static void
sparse_numbytes_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, NULL, 10, &u, "") == LONGINT_OK)
    {
      if (st->sparse_map_avail == st->sparse_map_size)
	{
	  size_t newsize = st->sparse_map_size *= 2;
	  st->sparse_map = xrealloc (st->sparse_map,
				     st->sparse_map_size
				     * sizeof st->sparse_map[0]);
	}
      st->sparse_map[st->sparse_map_avail++].numbytes = u;
    }
}

struct xhdr_tab const xhdr_tab[] = {
  { "atime",	atime_coder,	atime_decoder	},
  { "comment",	dummy_coder,	dummy_decoder	},
  { "charset",	dummy_coder,	dummy_decoder	},
  { "ctime",	ctime_coder,	ctime_decoder	},
  { "gid",	gid_coder,	gid_decoder	},
  { "gname",	gname_coder,	gname_decoder	},
  { "linkpath", linkpath_coder, linkpath_decoder},
  { "mtime",	mtime_coder,	mtime_decoder	},
  { "path",	path_coder,	path_decoder	},
  { "size",	size_coder,	size_decoder	},
  { "uid",	uid_coder,	uid_decoder	},
  { "uname",	uname_coder,	uname_decoder	},

  /* Sparse file handling */
  { "GNU.sparse.size",       sparse_size_coder, sparse_size_decoder, true },
  { "GNU.sparse.numblocks",  sparse_numblocks_coder, sparse_numblocks_decoder,
    true },
  { "GNU.sparse.offset",     sparse_offset_coder, sparse_offset_decoder,
    true },
  { "GNU.sparse.numbytes",   sparse_numbytes_coder, sparse_numbytes_decoder,
    true },

#if 0 /* GNU private keywords (not yet implemented) */

  /* The next directory entry actually contains the names of files
     that were in the directory at the time the dump was made.
     Supersedes GNUTYPE_DUMPDIR header type.  */
  { "GNU.dumpdir",  dumpdir_coder, dumpdir_decoder },

  /* Keeps the tape/volume header. May be present only in the global headers.
     Equivalent to GNUTYPE_VOLHDR.  */
  { "GNU.volume.header", volume_header_coder, volume_header_decoder },

  /* These may be present in a first global header of the archive.
     They provide the same functionality as GNUTYPE_MULTIVOL header.
     The GNU.volume.size keeps the real_s_sizeleft value, which is
     otherwise kept in the size field of a multivolume header.  The
     GNU.volume.offset keeps the offset of the start of this volume,
     otherwise kept in oldgnu_header.offset.  */
  { "GNU.volume.size", volume_size_coder, volume_size_decoder },
  { "GNU.volume.offset", volume_offset_coder, volume_offset_decoder },
#endif

  { NULL, NULL, NULL }
};

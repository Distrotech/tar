/* POSIX extended and STAR headers.

   Copyright (C) 2003 Free Software Foundation, Inc.

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

/* General Interface */

struct xhdr_tab
{
  char const *keyword;
  void (*coder) (struct tar_stat_info const *, char const *,
		 struct xheader *, void *data);
  void (*decoder) (struct tar_stat_info *, char const *);
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
      ERROR ((0, 0, _("Malformed extended header: missing whitespace after the length")));
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

void
xheader_decode (struct tar_stat_info *st)
{
  char *p = extended_header.buffer + BLOCKSIZE;
  char *endp = &extended_header.buffer[extended_header.size-1];

  while (p < endp)
    if (!decode_record (&p, st))
      break;
}

void
xheader_store (char const *keyword, struct tar_stat_info const *st, void *data)
{
  struct xhdr_tab const *t;

  if (extended_header.buffer)
    return;
  t = locate_handler (keyword);
  if (!t)
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
code_time (time_t t, char const *keyword, struct xheader *xhdr)
{
  char sbuf[100];
  size_t s = format_uintmax (t, NULL, 0);
  format_uintmax (t, sbuf, s);
  sbuf[s++] = '.';
  format_uintmax (0, sbuf + s, 9);
  sbuf[s+9] = 0;
  xheader_print (xhdr, keyword, sbuf);
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
  code_time (st->stat.st_atime, keyword, xhdr);
}

static void
atime_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
    st->stat.st_atime = u;
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  code_time (st->stat.st_ctime, keyword, xhdr);
}

static void
ctime_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
    st->stat.st_ctime = u;
}

static void
mtime_coder (struct tar_stat_info const *st, char const *keyword,
	     struct xheader *xhdr, void *data)
{
  code_time (st->stat.st_mtime, keyword, xhdr);
}

static void
mtime_decoder (struct tar_stat_info *st, char const *arg)
{
  uintmax_t u;
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
    st->stat.st_mtime = u;
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
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
  { "GNU.sparse.size",       sparse_size_coder, sparse_size_decoder },
  { "GNU.sparse.numblocks",  sparse_numblocks_coder, sparse_numblocks_decoder },
  { "GNU.sparse.offset",     sparse_offset_coder, sparse_offset_decoder },
  { "GNU.sparse.numbytes",   sparse_numbytes_coder, sparse_numbytes_decoder },

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

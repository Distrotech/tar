/* This file is part of GNU Tar

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

#include <grp.h>
#include <hash.h>
#include <pwd.h>
#include <quotearg.h>

#include "common.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
#include <obstack.h>

/* Forward declarations */
static void dummy_coder (struct tar_stat_info *st, char *keyword,
			 struct xheader *xhdr);
static void dummy_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void atime_coder (struct tar_stat_info *st, char *keyword,
			 struct xheader *xhdr); 
static void atime_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void gid_coder (struct tar_stat_info *st, char *keyword,
		       struct xheader *xhdr); 
static void gid_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void gname_coder (struct tar_stat_info *st, char *keyword,
			 struct xheader *xhdr); 
static void gname_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void linkpath_coder (struct tar_stat_info *st, char *keyword,
			    struct xheader *xhdr); 
static void linkpath_decoder (struct tar_stat_info *st, char *keyword, char *arg);  

static void mtime_coder (struct tar_stat_info *st, char *keyword,
			 struct xheader *xhdr); 
static void mtime_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void ctime_coder (struct tar_stat_info *st, char *keyword,
			 struct xheader *xhdr); 
static void ctime_decoder (struct tar_stat_info *st, char *keyword, char *arg);
     
static void path_coder (struct tar_stat_info *st, char *keyword,
			struct xheader *xhdr); 
static void path_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void size_coder (struct tar_stat_info *st, char *keyword,
			struct xheader *xhdr); 
static void size_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void uid_coder (struct tar_stat_info *st, char *keyword,
		       struct xheader *xhdr); 
static void uid_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void uname_coder (struct tar_stat_info *st, char *keyword,
			 struct xheader *xhdr); 
static void uname_decoder (struct tar_stat_info *st, char *keyword, char *arg);

/* General Interface */

struct xhdr_tab
{
  char *keyword;
  void (*coder) (struct tar_stat_info *st, char *keyword, struct xheader *xhdr);
  void (*decoder) (struct tar_stat_info *st, char *keyword, char *arg);
};

struct xhdr_tab xhdr_tab[] = {
  { "atime",	atime_coder,    atime_decoder },
  { "comment",	dummy_coder,    dummy_decoder },
  { "charset",	dummy_coder,    dummy_decoder },
  { "ctime",	ctime_coder,    ctime_decoder },
  { "gid",	gid_coder,      gid_decoder },
  { "gname",	gname_coder,    gname_decoder },
  { "linkpath",	linkpath_coder, linkpath_decoder },
  { "mtime",	mtime_coder,    mtime_decoder },
  { "path",	path_coder,     path_decoder },
  { "size",	size_coder,     size_decoder },
  { "uid",	uid_coder,      uid_decoder },
  { "uname",	uname_coder,    uname_decoder },    
  { NULL },
};

static struct xhdr_tab *
locate_handler (char *keyword)
{
  struct xhdr_tab *p;

  for (p = xhdr_tab; p->keyword; p++)
    if (strcmp (p->keyword, keyword) == 0)
      return p;
  return NULL;
}

static int
decode_record (char **p, struct tar_stat_info *st)
{
  size_t len;
  char *keyword, *eqp;
  char *start = *p;
  struct xhdr_tab *t;

  if (**p == 0)
    return 1;
  
  len = strtoul (*p, p, 10);
  if (**p != ' ')
    {
      ERROR ((0, 0, _("Malformed extended headed")));
      return 1;
    }
  
  keyword = ++*p;
  for (;*p < start + len; ++*p)
    if (**p == '=')
      break;

  if (**p != '=')
    {
      ERROR ((0, 0, _("Malformed extended headed")));
      return 1;
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
      t->decoder (st, keyword, value);
      start[len-1] = endc;
    }
  *eqp = '=';
  *p = &start[len];
  return 0;
}

void
xheader_decode (struct tar_stat_info *st)
{
  char *p, *endp;

  p = extended_header.buffer;
  endp = &extended_header.buffer[extended_header.size-1];

  while (p < endp)
    if (decode_record (&p, st))
      break;
}

void
xheader_store (char *keyword, struct tar_stat_info *st)
{
  struct xhdr_tab *t;

  if (extended_header.buffer)
    return;
  t = locate_handler (keyword);
  if (!t)
    return;
  if (!extended_header.stk)
    {
      extended_header.stk = xmalloc (sizeof (*extended_header.stk));
      obstack_init (extended_header.stk);
    }
  t->coder (st, keyword, &extended_header);
}

void
xheader_read (union block *p, size_t size)
{
  size_t i, j;
  size_t nblocks;
  
  free (extended_header.buffer);
  extended_header.size = size;
  nblocks = (size + BLOCKSIZE - 1) / BLOCKSIZE;
  extended_header.buffer = xmalloc (size + 1);

  set_next_block_after (p);
  for (i = j = 0; i < nblocks; i++)
    {
      size_t len;
      
      p = find_next_block ();
      len = size;
      if (len > BLOCKSIZE)
	len = BLOCKSIZE;
      memcpy (&extended_header.buffer[j], p->buffer, len);
      set_next_block_after (p);

      j += len;
      size -= len;
    }
}

size_t 
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

void
xheader_print (struct xheader *xhdr, char *keyword, char *value)
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
      free  (xhdr->stk);
      xhdr->stk = NULL;
    }
  else
    free (xhdr->buffer);
  xhdr->buffer = 0;
  xhdr->size = 0;
}


/* Implementations */
static void
code_string (char *string, char *keyword, struct xheader *xhdr)
{
  xheader_print (xhdr, keyword, string);
}

static void
code_time (time_t t, char *keyword, struct xheader *xhdr)
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
code_num (uintmax_t value, char *keyword, struct xheader *xhdr)
{
  char sbuf[100]; 
  size_t s = format_uintmax (value, NULL, 0);
  format_uintmax (value, sbuf, s);
  sbuf[s] = 0;
  xheader_print (xhdr, keyword, sbuf);
}

static void
dummy_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
}

static void
dummy_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
atime_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_time (st->stat.st_atime, keyword, xhdr);
}

static void
atime_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_atime = strtoul (arg, NULL, 0);
}

static void
gid_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_num (st->stat.st_gid, keyword, xhdr);
}

static void
gid_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_gid = strtoul (arg, NULL, 0);
}

static void
gname_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_string (st->gname, keyword, xhdr);
}

static void
gname_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  assign_string (&st->gname, arg);
}

static void
linkpath_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_string (st->link_name, keyword, xhdr);
}

static void
linkpath_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  assign_string (&st->link_name, arg);
}

static void
ctime_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_time (st->stat.st_ctime, keyword, xhdr);
}

static void
ctime_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_ctime = strtoul (arg, NULL, 0);
}

static void
mtime_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_time (st->stat.st_mtime, keyword, xhdr);
}

static void
mtime_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_mtime = strtoul (arg, NULL, 0);
}

static void
path_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_string (st->orig_file_name, keyword, xhdr);
}

static void
path_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  assign_string (&st->orig_file_name, arg);
  assign_string (&st->file_name, arg);
  st->had_trailing_slash = strip_trailing_slashes (st->file_name);
}

static void
size_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_num (st->stat.st_size, keyword, xhdr);
}

static void
size_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_size = strtoul (arg, NULL, 0);
}

static void
uid_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_num (st->stat.st_uid, keyword, xhdr);
}

static void
uid_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_uid = strtoul (arg, NULL, 0);
}

static void
uname_coder (struct tar_stat_info *st, char *keyword, struct xheader *xhdr)
{
  code_string (st->uname, keyword, xhdr);
}

static void
uname_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  assign_string (&st->uname, arg);
}
  

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

/* Forward declarations */
static void dummy_handler (struct tar_stat_info *st, char *keyword, char *arg);

static void atime_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void atime_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void gid_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void gid_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void gname_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void gname_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void linkpath_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void linkpath_decoder (struct tar_stat_info *st, char *keyword, char *arg);  

static void mtime_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void mtime_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void ctime_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void ctime_decoder (struct tar_stat_info *st, char *keyword, char *arg);
     
static void path_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void path_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void size_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void size_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void uid_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void uid_decoder (struct tar_stat_info *st, char *keyword, char *arg);

static void uname_coder (struct tar_stat_info *st, char *keyword, char *arg); 
static void uname_decoder (struct tar_stat_info *st, char *keyword, char *arg);

/* General Interface */

struct xhdr_tab
{
  char *keyword;
  void (*coder) (struct tar_stat_info *st, char *keyword, char *arg);
  void (*decoder) (struct tar_stat_info *st, char *keyword, char *arg);
};

struct xhdr_tab xhdr_tab[] = {
  { "atime",	atime_coder,    atime_decoder },
  { "comment",	dummy_handler,  dummy_handler },
  { "charset",	dummy_handler,  dummy_handler },
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

  p = extended_header.blocks->buffer;
  endp = &extended_header.blocks[extended_header.nblocks-1].buffer
                                        [sizeof(extended_header.blocks[0])-1];
  while (p < endp)
    if (decode_record (&p, st))
      break;
}

void
xheader_read (union block *p, size_t size)
{
  size_t i;
  
  free (extended_header.blocks);
  extended_header.nblocks = (size + BLOCKSIZE - 1) / BLOCKSIZE;
  extended_header.blocks = xmalloc (sizeof (extended_header.blocks[0]) *
					    extended_header.nblocks);
  set_next_block_after (p);
  for (i = 0; i < extended_header.nblocks; i++)
    {
      p = find_next_block ();
      memcpy (&extended_header.blocks[i], p, sizeof (p[0]));
      set_next_block_after (p);
    }
}


/* Implementations */
static void
dummy_handler (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
atime_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
atime_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_atime = strtoul (arg, NULL, 0);
}

static void
gid_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
gid_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_gid = strtoul (arg, NULL, 0);
}

static void
gname_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
gname_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->gname = strdup (arg);
}

static void
linkpath_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
linkpath_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->link_name = strdup (arg);
}

static void
ctime_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
ctime_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_ctime = strtoul (arg, NULL, 0);
}

static void
mtime_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
mtime_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_mtime = strtoul (arg, NULL, 0);
}

static void
path_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
path_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  assign_string (&st->orig_file_name, arg);
  assign_string (&st->file_name, arg);
  st->had_trailing_slash = strip_trailing_slashes (st->file_name);
}

static void
size_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
size_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_size = strtoul (arg, NULL, 0);
}

static void
uid_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
uid_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->stat.st_uid = strtoul (arg, NULL, 0);
}

static void
uname_coder (struct tar_stat_info *st, char *keyword, char *arg)
{
}

static void
uname_decoder (struct tar_stat_info *st, char *keyword, char *arg)
{
  st->uname = strdup (arg);
}
  

/* Support for extended attributes.

   Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Free Software
   Foundation, Inc.

   Written by James Antill, on 2006-07-27.

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
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <system.h>

#include <fnmatch.h>
#include <quotearg.h>

#include "common.h"

#include "xattr-at.h"

struct xattrs_mask_map
{
  const char **masks;
  int size;
  int used;
};

/* list of fnmatch patterns */
static struct
{
  /* lists of fnmatch patterns */
  struct xattrs_mask_map incl;
  struct xattrs_mask_map excl;
} xattrs_setup;

static void mask_map_realloc (struct xattrs_mask_map *map)
{
  if (map->size == 0)
    {
      map->size = 4;
      map->masks = xmalloc (16 * sizeof (char *));
      return;
    }

  if (map->size <= map->used)
    {
      map->size *= 2;
      map->masks = xrealloc (map->masks, map->size * sizeof (char *));
      return;
    }
}

void xattrs_mask_add (const char *mask, bool incl)
{
  struct xattrs_mask_map *mask_map = incl ? &xattrs_setup.incl
                                          : &xattrs_setup.excl;
  /* ensure there is enough space */
  mask_map_realloc (mask_map);
  /* just assign pointers -- we silently expect that pointer "mask" is valid
     through the whole program (pointer to argv array) */
  mask_map->masks[mask_map->used++] = mask;
}

static void clear_mask_map (struct xattrs_mask_map *mask_map)
{
  if (mask_map->size)
    free (mask_map->masks);
}

void xattrs_clear_setup ()
{
  clear_mask_map (&xattrs_setup.incl);
  clear_mask_map (&xattrs_setup.excl);
}

/* get all xattrs from file given by FILE_NAME or FD (when non-zero).  This
   includes all the user.*, security.*, system.*, etc. available domains */
void xattrs_xattrs_get (int parentfd, char const *file_name,
                        struct tar_stat_info *st, int fd)
{
  if (xattrs_option > 0)
    {
#ifndef HAVE_XATTRS
      static int done = 0;
      if (!done)
        WARN ((0, 0, _("XATTR support is not available")));
      done = 1;
#else
      static ssize_t xsz = 1024;
      static char *xatrs = NULL;
      ssize_t xret = -1;

      if (!xatrs) xatrs = xmalloc (xsz);

      while (((fd == 0) ?
              ((xret = llistxattrat (parentfd, file_name, xatrs, xsz)) == -1) :
              ((xret = flistxattr (fd, xatrs, xsz)) == -1)) &&
             (errno == ERANGE))
        {
          xsz <<= 1;
          xatrs = xrealloc (xatrs, xsz);
        }

      if (xret == -1)
        call_arg_warn ((fd == 0) ? "llistxattrat" : "flistxattr", file_name);
      else
        {
          const char *attr = xatrs;
          static ssize_t asz = 1024;
          static char *val = NULL;

          if (!val) val = xmalloc (asz);

          while (xret > 0)
            {
              size_t len = strlen (attr);
              ssize_t aret = 0;

              /* Archive all xattrs during creation, decide at extraction time
               * which ones are of interest/use for the target filesystem. */
              while (((fd == 0)
                      ? ((aret = lgetxattrat (parentfd, file_name, attr,
                                              val, asz)) == -1)
                      : ((aret = fgetxattr (fd, attr, val, asz)) == -1))
                     && (errno == ERANGE))
                {
                  asz <<= 1;
                  val = xrealloc (val, asz);
                }

              if (aret != -1)
                xheader_xattr_add (st, attr, val, aret);
              else if (errno != ENOATTR)
                call_arg_warn ((fd == 0) ? "lgetxattrat"
                                         : "fgetxattr", file_name);

              attr += len + 1;
              xret -= len + 1;
            }
        }
#endif
    }
}

static void xattrs__fd_set (struct tar_stat_info const *st,
                            char const *file_name, char typeflag,
                            const char *attr,
                            const char *ptr, size_t len)
{
  if (ptr)
    {
      const char *sysname = "setxattrat";
      int ret = -1;

      if (typeflag != SYMTYPE)
        ret = setxattrat (chdir_fd, file_name, attr, ptr, len, 0);
      else
        {
          sysname = "lsetxattr";
          ret = lsetxattrat (chdir_fd, file_name, attr, ptr, len, 0);
        }

      if (ret == -1)
        WARNOPT (WARN_XATTR_WRITE, (0, errno,
            _("%s: Cannot set '%s' extended attribute for file '%s'"),
            sysname, attr, file_name));
    }
}

static bool xattrs_matches_mask (const char *kw, struct xattrs_mask_map *mm)
{
  int i;

  if (!mm->size)
    return false;

  for (i = 0; i < mm->used; i++)
    if (fnmatch (mm->masks[i], kw, 0) == 0)
      return true;

  return false;
}

static bool xattrs_kw_included (const char *kw, bool archiving)
{
   if (xattrs_setup.incl.size)
     return xattrs_matches_mask (kw, &xattrs_setup.incl);
   else
     {
       if (archiving)
         return true;
       else
         return strncmp (kw, "user.", strlen ("user.")) == 0;
     }
}

static bool xattrs_kw_excluded (const char *kw, bool archiving)
{
  if (!xattrs_setup.excl.size)
    return false;

  return xattrs_matches_mask (kw, &xattrs_setup.excl);
}

/* Check whether the xattr with keyword KW should be discarded from list of
   attributes that are going to be archived/excluded (set ARCHIVING=true for
   archiving, false for excluding) */
static bool xattrs_masked_out (const char *kw, bool archiving)
{
  if (!xattrs_kw_included (kw, archiving))
    return true;

  return xattrs_kw_excluded (kw, archiving);
}

void xattrs_xattrs_set (struct tar_stat_info const *st,
                        char const *file_name, char typeflag,
                        int later_run)
{
  if (xattrs_option > 0)
    {
#ifndef HAVE_XATTRS
      static int done = 0;
      if (!done)
        WARN ((0, 0, _("XATTR support is not available")));
      done = 1;
#else
      size_t scan = 0;

      if (!st->xattr_map_size)
        return;

      for (; scan < st->xattr_map_size; ++scan)
        {
          char *keyword = st->xattr_map[scan].xkey;
          keyword += strlen ("SCHILY.xattr.");

          /* TODO: this 'later_run' workaround is temporary solution -> once
             capabilities should become fully supported by it's API and there
             should exist something like xattrs_capabilities_set() call.
             For a regular files: all extended attributes are restored during
             the first run except 'security.capability' which is restored in
             'later_run == 1'.  */
          if (typeflag == REGTYPE
              && later_run == !!strcmp (keyword, "security.capability"))
            continue;

          if (xattrs_masked_out (keyword, false /* extracting */ ))
            /* we don't want to restore this keyword */
            continue;

          xattrs__fd_set (st, file_name, typeflag, keyword,
                          st->xattr_map[scan].xval_ptr,
                          st->xattr_map[scan].xval_len);
        }
#endif
    }
}

void xattrs_print_char (struct tar_stat_info const *st, char *output)
{
  int i;
  if (verbose_option < 2)
    {
      *output = 0;
      return;
    }

  if (xattrs_option > 0)
    {
      /* placeholders */
      *output = ' ';
      *(output + 1) = 0;
    }

  if (xattrs_option > 0 && st->xattr_map_size)
    for (i = 0; i < st->xattr_map_size; ++i)
      {
        char *keyword = st->xattr_map[i].xkey + strlen ("SCHILY.xattr.");
        if (xattrs_masked_out (keyword, false /* like extracting */ ))
          continue;
        *output = '*';
        break;
      }
}

void xattrs_print (struct tar_stat_info const *st)
{
  if (verbose_option < 3)
    return;

  /* xattrs */
  if (xattrs_option && st->xattr_map_size)
    {
      int i;
      for (i = 0; i < st->xattr_map_size; ++i)
        {
          char *keyword = st->xattr_map[i].xkey + strlen ("SCHILY.xattr.");
          if (xattrs_masked_out (keyword, false /* like extracting */ ))
            continue;
          fprintf (stdlis, "  x: %lu %s\n",
		   (unsigned long) st->xattr_map[i].xval_len, keyword);
        }
    }
}

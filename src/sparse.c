/* Functions for dealing with sparse files 

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
#include <quotearg.h>
#include "common.h"

struct tar_sparse_file;

enum sparse_scan_state
  {
    scan_begin,
    scan_block,
    scan_end
  };

struct tar_sparse_optab
{
  bool (*init) (struct tar_sparse_file *);
  bool (*done) (struct tar_sparse_file *);
  bool (*dump_header) (struct tar_sparse_file *);
  bool (*decode_header) (struct tar_sparse_file *);
  bool (*scan_block) (struct tar_sparse_file *, enum sparse_scan_state,
		      void *);
  bool (*dump_region) (struct tar_sparse_file *, size_t index);
  bool (*extract_region) (struct tar_sparse_file *, size_t index);
};

struct tar_sparse_file
{
  int fd;                           /* File descriptor */
  size_t dumped_size;               /* Number of bytes actually written
				       to the archive */
  struct tar_stat_info *stat_info;  /* Information about the file */
  struct tar_sparse_optab *optab;
  void *closure;                    /* Any additional data optab calls might
				       reqiure */
};

static bool
tar_sparse_init (struct tar_sparse_file *file)
{
  file->dumped_size = 0;
  if (file->optab->init)
    return file->optab->init (file);
  return true;
}

static bool
tar_sparse_done (struct tar_sparse_file *file)
{
  if (file->optab->done)
    return file->optab->done (file);
  return true;
}

static bool
tar_sparse_scan (struct tar_sparse_file *file, enum sparse_scan_state state,
		 void *block)
{
  if (file->optab->scan_block)
    return file->optab->scan_block (file, state, block);
  return true;
}

static bool
tar_sparse_dump_region (struct tar_sparse_file *file, size_t index)
{
  if (file->optab->dump_region)
    return file->optab->dump_region (file, index);
  return false;
}

static bool
tar_sparse_extract_region (struct tar_sparse_file *file, size_t index)
{
  if (file->optab->extract_region)
    return file->optab->extract_region (file, index);
  return false;
}

static bool
tar_sparse_dump_header (struct tar_sparse_file *file)
{
  if (file->optab->dump_header)
    return file->optab->dump_header (file);
  return false;
}

static bool
tar_sparse_decode_header (struct tar_sparse_file *file)
{
  if (file->optab->decode_header)
    return file->optab->decode_header (file);
  return false;
}


static bool
lseek_or_error (struct tar_sparse_file *file, off_t offset, int whence)
{
  if (lseek (file->fd, offset, whence) < 0)
    {
      seek_diag_details (file->stat_info->orig_file_name, offset);
      return false;
    }
  return true;
}

/* Takes a blockful of data and basically cruises through it to see if
   it's made *entirely* of zeros, returning a 0 the instant it finds
   something that is a nonzero, i.e., useful data.  */
static bool
zero_block_p (char *buffer, size_t size)
{
  while (size--)
    if (*buffer++)
      return 0;
  return 1;
}

#define clear_block(p) memset (p, 0, BLOCKSIZE);

#define SPARSES_INIT_COUNT SPARSES_IN_SPARSE_HEADER

static void
sparse_add_map (struct tar_sparse_file *file, struct sp_array *sp)
{
  if (file->stat_info->sparse_map == NULL)
    {
      file->stat_info->sparse_map =
	xmalloc (SPARSES_INIT_COUNT * sizeof file->stat_info->sparse_map[0]);
      file->stat_info->sparse_map_size = SPARSES_INIT_COUNT;
    }
  else if (file->stat_info->sparse_map_avail == file->stat_info->sparse_map_size)
    {
      file->stat_info->sparse_map_size *= 2;
      file->stat_info->sparse_map =
	xrealloc (file->stat_info->sparse_map,
		  file->stat_info->sparse_map_size
		  * sizeof file->stat_info->sparse_map[0]);
    }
  file->stat_info->sparse_map[file->stat_info->sparse_map_avail++] = *sp;
}

/* Scan the sparse file and create its map */
static bool
sparse_scan_file (struct tar_sparse_file *file)
{
  static char buffer[BLOCKSIZE];
  size_t count;
  size_t offset = 0;
  struct sp_array sp = {0, 0};

  if (!lseek_or_error (file, 0, SEEK_SET))
    return false;
  clear_block (buffer);

  file->stat_info->sparse_map_size = 0;
  file->stat_info->archive_file_size = 0;
  
  if (!tar_sparse_scan (file, scan_begin, NULL))
    return false;

  while ((count = safe_read (file->fd, buffer, sizeof buffer)) > 0)
    {
      /* Analize the block */
      if (zero_block_p (buffer, count))
	{
	  if (sp.numbytes)
	    {
	      sparse_add_map (file, &sp);
	      sp.numbytes = 0;
	      if (!tar_sparse_scan (file, scan_block, NULL))
		return false;
	    }
	}
      else
	{
	  if (sp.numbytes == 0)
	    sp.offset = offset;
	  sp.numbytes += count;
	  file->stat_info->archive_file_size += count;
	  if (!tar_sparse_scan (file, scan_block, buffer))
	    return false;
	}
      
      offset += count;
      clear_block (buffer);
    }
      
  if (sp.numbytes == 0)
    {
      sp.offset = offset - 1;
      sp.numbytes = 1;
    }
  sparse_add_map (file, &sp);
  file->stat_info->archive_file_size += count;
  return tar_sparse_scan (file, scan_end, NULL);
}

static struct tar_sparse_optab oldgnu_optab;

static bool
sparse_select_optab (struct tar_sparse_file *file)
{
  switch (archive_format)
    {
    case V7_FORMAT:
    case USTAR_FORMAT:
      return false;

    case OLDGNU_FORMAT:
    case GNU_FORMAT: /*FIXME: This one should disappear? */
      file->optab = &oldgnu_optab;
      break;

    case POSIX_FORMAT:
    case STAR_FORMAT:
      /* FIXME: Add methods */
      return false;

    default:
      break;
    }
  return true;
}

static bool
sparse_dump_region (struct tar_sparse_file *file, size_t index)
{
  union block *blk;
  off_t bytes_left = file->stat_info->sparse_map[index].numbytes;
  
  if (!lseek_or_error (file, file->stat_info->sparse_map[index].offset,
		       SEEK_SET))
    return false;

  do
    {
      size_t bufsize = (bytes_left > BLOCKSIZE) ? BLOCKSIZE : bytes_left;
      off_t bytes_read;
      
      blk = find_next_block ();
      memset (blk->buffer, 0, BLOCKSIZE);
      bytes_read = safe_read (file->fd, blk->buffer, bufsize);
      if (bytes_read < 0)
	{
          read_diag_details (file->stat_info->orig_file_name,
	                     file->stat_info->sparse_map[index].offset
	                         + file->stat_info->sparse_map[index].numbytes
	                         - bytes_left,
	             bufsize);
	  return false;
	}

      bytes_left -= bytes_read;
      file->dumped_size += bytes_read;
      set_next_block_after (blk);
    }
  while (bytes_left > 0);
  return true;
}

static bool
sparse_extract_region (struct tar_sparse_file *file, size_t index)
{
  size_t write_size;
  
  if (!lseek_or_error (file, file->stat_info->sparse_map[index].offset,
		       SEEK_SET))
    return false;
  write_size = file->stat_info->sparse_map[index].numbytes;
  while (write_size > 0)
    {
      size_t count;
      size_t wrbytes = (write_size > BLOCKSIZE) ? BLOCKSIZE : write_size;
      union block *blk = find_next_block ();
      if (!blk)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  return false;
	}
      set_next_block_after (blk);
      count = full_write (file->fd, blk->buffer, wrbytes);
      write_size -= count;
      file->dumped_size += count;
      if (count != wrbytes)
	{
	  write_error_details (file->stat_info->orig_file_name,
			       count, wrbytes);
	  return false;
	}
    }
  return true;
}



/* Interface functions */
enum dump_status
sparse_dump_file (int fd, struct tar_stat_info *stat)
{
  bool rc;
  struct tar_sparse_file file;

  file.stat_info = stat;
  file.fd = fd;

  if (!sparse_select_optab (&file)
      || !tar_sparse_init (&file))
    return dump_status_not_implemented;

  rc = sparse_scan_file (&file);
  if (rc && file.optab->dump_region)
    {
      tar_sparse_dump_header (&file);

      if (fd >= 0)
	{
	  size_t i;

	  for (i = 0; rc && i < file.stat_info->sparse_map_avail; i++)
	    rc = tar_sparse_dump_region (&file, i);
	}
    }

  pad_archive(file.stat_info->archive_file_size - file.dumped_size);
  return (tar_sparse_done (&file) && rc) ? dump_status_ok : dump_status_short;
}

/* Returns true if the file represented by stat is a sparse one */
bool
sparse_file_p (struct tar_stat_info *stat)
{
  return (ST_NBLOCKS (stat->stat)
	  < (stat->stat.st_size / ST_NBLOCKSIZE
	     + (stat->stat.st_size % ST_NBLOCKSIZE != 0)));
}

enum dump_status
sparse_extract_file (int fd, struct tar_stat_info *stat, off_t *size)
{
  bool rc = true;
  struct tar_sparse_file file;
  size_t i;
  
  file.stat_info = stat;
  file.fd = fd;

  if (!sparse_select_optab (&file)
      || !tar_sparse_init (&file))
    return dump_status_not_implemented;

  rc = tar_sparse_decode_header (&file);
  for (i = 0; rc && i < file.stat_info->sparse_map_avail; i++)
    rc = tar_sparse_extract_region (&file, i);
  *size = file.stat_info->archive_file_size - file.dumped_size;
  return (tar_sparse_done (&file) && rc) ? dump_status_ok : dump_status_short;
}

     
/* Old GNU Format. The sparse file information is stored in the
   oldgnu_header in the following manner:

   The header is marked with type 'S'. Its `size' field contains
   the cumulative size of all non-empty blocks of the file. The
   actual file size is stored in `realsize' member of oldgnu_header.
   
   The map of the file is stored in a list of `struct sparse'.
   Each struct contains offset to the block of data and its
   size (both as octal numbers). The first file header contains
   at most 4 such structs (SPARSES_IN_OLDGNU_HEADER). If the map
   contains more structs, then the field `isextended' of the main
   header is set to 1 (binary) and the `struct sparse_header'
   header follows, containing at most 21 following structs
   (SPARSES_IN_SPARSE_HEADER). If more structs follow, `isextended'
   field of the extended header is set and next  next extension header
   follows, etc... */

enum oldgnu_add_status
  {
    add_ok,
    add_finish,
    add_fail
  };

/* Add a sparse item to the sparse file and its obstack */
static enum oldgnu_add_status 
oldgnu_add_sparse (struct tar_sparse_file *file, struct sparse *s)
{
  struct sp_array sp;

  if (s->numbytes[0] == '\0')
    return add_finish;
  sp.offset = OFF_FROM_HEADER (s->offset);
  sp.numbytes = SIZE_FROM_HEADER (s->numbytes);
  if (sp.offset < 0
      || file->stat_info->stat.st_size < sp.offset + sp.numbytes
      || file->stat_info->archive_file_size < 0)
    return add_fail;

  sparse_add_map (file, &sp);
  return add_ok;
}

/* Convert old GNU format sparse data to internal representation
   FIXME: Clubbers current_header! */
static bool
oldgnu_get_sparse_info (struct tar_sparse_file *file)
{
  size_t i;
  union block *h = current_header;
  int ext_p;
  static enum oldgnu_add_status rc;
  
  /* FIXME: note this! st_size was initialized from the header
     which actually contains archived size. The following fixes it */
  file->stat_info->archive_file_size = file->stat_info->stat.st_size;
  file->stat_info->stat.st_size =
                OFF_FROM_HEADER (current_header->oldgnu_header.realsize);
  
  file->stat_info->sparse_map_size = 0;
  for (i = 0; i < SPARSES_IN_OLDGNU_HEADER; i++)
    {
      rc = oldgnu_add_sparse (file, &h->oldgnu_header.sp[i]);
      if (rc != add_ok)
	break;
    }

  for (ext_p = h->oldgnu_header.isextended;
       rc == add_ok && ext_p; ext_p = h->sparse_header.isextended)
    {
      h = find_next_block ();
      if (!h)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  return false;
	}
      set_next_block_after (h);
      for (i = 0; i < SPARSES_IN_SPARSE_HEADER && rc == add_ok; i++)
	rc = oldgnu_add_sparse (file, &h->sparse_header.sp[i]);
    }

  if (rc == add_fail)
    {
      ERROR ((0, 0, _("%s: invalid sparse archive member"),
	      file->stat_info->orig_file_name));
      return false;
    }
  return true;
}

static void
oldgnu_store_sparse_info (struct tar_sparse_file *file, size_t *pindex,
			  struct sparse *sp, size_t sparse_size)
{
  for (; *pindex < file->stat_info->sparse_map_avail
	 && sparse_size > 0; sparse_size--, sp++, ++*pindex)
    {
      OFF_TO_CHARS (file->stat_info->sparse_map[*pindex].offset,
		    sp->offset);
      SIZE_TO_CHARS (file->stat_info->sparse_map[*pindex].numbytes,
		     sp->numbytes);
    }
}

static bool
oldgnu_dump_header (struct tar_sparse_file *file)
{
  off_t block_ordinal = current_block_ordinal ();
  union block *blk;
  size_t i;
    
  blk = start_header (file->stat_info);
  blk->header.typeflag = GNUTYPE_SPARSE;
  if (file->stat_info->sparse_map_avail > SPARSES_IN_OLDGNU_HEADER)
    blk->oldgnu_header.isextended = 1;

  /* Store the real file size */
  OFF_TO_CHARS (file->stat_info->stat.st_size, blk->oldgnu_header.realsize);
  /* Store the effective (shrunken) file size */
  OFF_TO_CHARS (file->stat_info->archive_file_size, blk->header.size);

  i = 0;
  oldgnu_store_sparse_info (file, &i,
			    blk->oldgnu_header.sp,
			    SPARSES_IN_OLDGNU_HEADER);
  blk->oldgnu_header.isextended = i < file->stat_info->sparse_map_avail;
  finish_header (file->stat_info, blk, block_ordinal);
  
  while (i < file->stat_info->sparse_map_avail)
    {
      blk = find_next_block ();
      memset (blk->buffer, 0, BLOCKSIZE);
      oldgnu_store_sparse_info (file, &i,
				blk->sparse_header.sp,
				SPARSES_IN_SPARSE_HEADER);
      set_next_block_after (blk);
      if (i < file->stat_info->sparse_map_avail)
	blk->sparse_header.isextended = 1;
      else
	break;
    }
  return true;
}

static struct tar_sparse_optab oldgnu_optab = {
  NULL,  /* No init function */
  NULL,  /* No done function */
  oldgnu_dump_header,
  oldgnu_get_sparse_info,
  NULL,  /* No scan_block function */
  sparse_dump_region,
  sparse_extract_region,
};

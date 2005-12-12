/* GNU dump extensions to tar.

   Copyright (C) 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001,
   2003, 2004, 2005 Free Software Foundation, Inc.

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
#include <getline.h>
#include <hash.h>
#include <quotearg.h>
#include "common.h"

/* Incremental dump specialities.  */

/* Which child files to save under a directory.  */
enum children {NO_CHILDREN, CHANGED_CHILDREN, ALL_CHILDREN};

/* Directory attributes.  */
struct directory
  {
    struct timespec mtime;      /* Modification time */
    dev_t device_number;	/* device number for directory */
    ino_t inode_number;		/* inode number for directory */
    enum children children;
    bool nfs;
    bool found;
    char name[1];		/* file name of directory */
  };

static Hash_table *directory_table;

#if HAVE_ST_FSTYPE_STRING
  static char const nfs_string[] = "nfs";
# define NFS_FILE_STAT(st) (strcmp ((st).st_fstype, nfs_string) == 0)
#else
# define ST_DEV_MSB(st) (~ (dev_t) 0 << (sizeof (st).st_dev * CHAR_BIT - 1))
# define NFS_FILE_STAT(st) (((st).st_dev & ST_DEV_MSB (st)) != 0)
#endif

/* Calculate the hash of a directory.  */
static size_t
hash_directory (void const *entry, size_t n_buckets)
{
  struct directory const *directory = entry;
  return hash_string (directory->name, n_buckets);
}

/* Compare two directories for equality.  */
static bool
compare_directories (void const *entry1, void const *entry2)
{
  struct directory const *directory1 = entry1;
  struct directory const *directory2 = entry2;
  return strcmp (directory1->name, directory2->name) == 0;
}

/* Create and link a new directory entry for directory NAME, having a
   device number DEV and an inode number INO, with NFS indicating
   whether it is an NFS device and FOUND indicating whether we have
   found that the directory exists.  */
static struct directory *
note_directory (char const *name, struct timespec mtime,
		dev_t dev, ino_t ino, bool nfs, bool found)
{
  size_t size = offsetof (struct directory, name) + strlen (name) + 1;
  struct directory *directory = xmalloc (size);

  directory->mtime = mtime;
  directory->device_number = dev;
  directory->inode_number = ino;
  directory->children = CHANGED_CHILDREN;
  directory->nfs = nfs;
  directory->found = found;
  strcpy (directory->name, name);

  if (! ((directory_table
	  || (directory_table = hash_initialize (0, 0, hash_directory,
						 compare_directories, 0)))
	 && hash_insert (directory_table, directory)))
    xalloc_die ();

  return directory;
}

/* Return a directory entry for a given file NAME, or zero if none found.  */
static struct directory *
find_directory (char *name)
{
  if (! directory_table)
    return 0;
  else
    {
      size_t size = offsetof (struct directory, name) + strlen (name) + 1;
      struct directory *dir = alloca (size);
      strcpy (dir->name, name);
      return hash_lookup (directory_table, dir);
    }
}

void
update_parent_directory (const char *name)
{
  struct directory *directory;
  char *p, *name_buffer;
  
  p = dir_name (name);
  name_buffer = xmalloc (strlen (p) + 2);
  strcpy (name_buffer, p);
  if (! ISSLASH (p[strlen (p) - 1]))
    strcat (name_buffer, "/");
  
  directory = find_directory (name_buffer);
  free (name_buffer);
  if (directory)
    {
      struct stat st;
      if (deref_stat (dereference_option, p, &st) != 0)
	stat_diag (name);
      else
	directory->mtime = get_stat_mtime (&st);
    }
  free (p);
}

static int
compare_dirents (const void *first, const void *second)
{
  return strcmp ((*(char *const *) first) + 1,
		 (*(char *const *) second) + 1);
}

enum children 
procdir (char *name_buffer, struct stat *stat_data,
	 dev_t device,
	 enum children children,
	 bool verbose)
{ 
  struct directory *directory;
  bool nfs = NFS_FILE_STAT (*stat_data);
  
  if ((directory = find_directory (name_buffer)) != NULL)
    {
      /* With NFS, the same file can have two different devices
	 if an NFS directory is mounted in multiple locations,
	 which is relatively common when automounting.
	 To avoid spurious incremental redumping of
	 directories, consider all NFS devices as equal,
	 relying on the i-node to establish differences.  */
      
      if (! (((directory->nfs & nfs)
	      || directory->device_number == stat_data->st_dev)
	     && directory->inode_number == stat_data->st_ino))
	{
	  if (verbose)
	    WARN ((0, 0, _("%s: Directory has been renamed"),
		   quotearg_colon (name_buffer)));
	  directory->children = ALL_CHILDREN;
	  directory->nfs = nfs;
	  directory->device_number = stat_data->st_dev;
	  directory->inode_number = stat_data->st_ino;
	}
      else if (listed_incremental_option)
	/* Newer modification time can mean that new files were
	   created in the directory or some of the existing files
	   were renamed. */
	directory->children =
	  timespec_cmp (get_stat_mtime (stat_data), directory->mtime) > 0
	  ? ALL_CHILDREN : CHANGED_CHILDREN;

      directory->found = true;
    }
  else
    {
      if (verbose)
	WARN ((0, 0, _("%s: Directory is new"),
	       quotearg_colon (name_buffer)));
      directory = note_directory (name_buffer,
				  get_stat_mtime(stat_data),
				  stat_data->st_dev,
				  stat_data->st_ino,
				  nfs,
				  true);

      directory->children =
	(listed_incremental_option
	 || (OLDER_STAT_TIME (*stat_data, m)
	     || (after_date_option
		 && OLDER_STAT_TIME (*stat_data, c))))
	? ALL_CHILDREN
	: CHANGED_CHILDREN;
    }

  /* If the directory is on another device and --one-file-system was given,
     omit it... */
  if (one_file_system_option && device != stat_data->st_dev
      /* ... except if it was explicitely given in the command line */
      && !name_scan (name_buffer))
    directory->children = NO_CHILDREN;
  else if (children == ALL_CHILDREN)
    directory->children = ALL_CHILDREN;
  
  return directory->children;
}


/* Recursively scan the given directory. */
static void
scan_directory (struct obstack *stk, char *dir_name, dev_t device)
{
  char *dirp = savedir (dir_name);	/* for scanning directory */
  char const *entry;	/* directory entry being scanned */
  size_t entrylen;	/* length of directory entry */
  char *name_buffer;		/* directory, `/', and directory member */
  size_t name_buffer_size;	/* allocated size of name_buffer, minus 2 */
  size_t name_length;		/* used length in name_buffer */
  enum children children;
  struct stat stat_data;

  if (! dirp)
    savedir_error (dir_name);

  name_buffer_size = strlen (dir_name) + NAME_FIELD_SIZE;
  name_buffer = xmalloc (name_buffer_size + 2);
  strcpy (name_buffer, dir_name);
  if (! ISSLASH (dir_name[strlen (dir_name) - 1]))
    strcat (name_buffer, "/");
  name_length = strlen (name_buffer);

  if (deref_stat (dereference_option, name_buffer, &stat_data))
    {
      stat_diag (name_buffer);
      children = CHANGED_CHILDREN;
    }
  else
    children = procdir (name_buffer, &stat_data, device, NO_CHILDREN, false);
  
  if (dirp && children != NO_CHILDREN)
    for (entry = dirp;
	 (entrylen = strlen (entry)) != 0;
	 entry += entrylen + 1)
      {
	if (name_buffer_size <= entrylen + name_length)
	  {
	    do
	      name_buffer_size += NAME_FIELD_SIZE;
	    while (name_buffer_size <= entrylen + name_length);
	    name_buffer = xrealloc (name_buffer, name_buffer_size + 2);
	  }
	strcpy (name_buffer + name_length, entry);

	if (excluded_name (name_buffer))
	  obstack_1grow (stk, 'N');
	else
	  {

	    if (deref_stat (dereference_option, name_buffer, &stat_data))
	      {
		stat_diag (name_buffer);
		continue;
	      }

	    if (S_ISDIR (stat_data.st_mode))
	      {
		procdir (name_buffer, &stat_data, device, children,
			 verbose_option);
		obstack_1grow (stk, 'D');
	      }

	    else if (one_file_system_option && device != stat_data.st_dev)
	      obstack_1grow (stk, 'N');

#ifdef S_ISHIDDEN
	    else if (S_ISHIDDEN (stat_data.st_mode))
	      {
		obstack_1grow (stk, 'D');
		obstack_grow (stk, entry, entrylen);
		obstack_grow (stk, "A", 2);
		continue;
	      }
#endif

	    else
	      if (children == CHANGED_CHILDREN
		  && OLDER_STAT_TIME (stat_data, m)
		  && (!after_date_option || OLDER_STAT_TIME (stat_data, c)))
		obstack_1grow (stk, 'N');
	      else
		obstack_1grow (stk, 'Y');
	  }

	obstack_grow (stk, entry, entrylen + 1);
      }

  obstack_grow (stk, "\000\000", 2);

  free (name_buffer);
  if (dirp)
    free (dirp);
}

/* Sort the contents of the obstack, and convert it to the char * */
static char *
sort_obstack (struct obstack *stk)
{
  char *pointer = obstack_finish (stk);
  size_t counter;
  char *cursor;
  char *buffer;
  char **array;
  char **array_cursor;

  counter = 0;
  for (cursor = pointer; *cursor; cursor += strlen (cursor) + 1)
    counter++;

  if (!counter)
    return NULL;

  array = obstack_alloc (stk, sizeof (char *) * (counter + 1));

  array_cursor = array;
  for (cursor = pointer; *cursor; cursor += strlen (cursor) + 1)
    *array_cursor++ = cursor;
  *array_cursor = 0;

  qsort (array, counter, sizeof (char *), compare_dirents);

  buffer = xmalloc (cursor - pointer + 2);

  cursor = buffer;
  for (array_cursor = array; *array_cursor; array_cursor++)
    {
      char *string = *array_cursor;

      while ((*cursor++ = *string++))
	continue;
    }
  *cursor = '\0';
  return buffer;
}

char *
get_directory_contents (char *dir_name, dev_t device)
{
  struct obstack stk;
  char *buffer;

  obstack_init (&stk);
  scan_directory (&stk, dir_name, device);
  buffer = sort_obstack (&stk);
  obstack_free (&stk, NULL);
  return buffer;
}

size_t
dumpdir_size (const char *p)
{
  size_t totsize = 0;

  while (*p)
    {
      size_t size = strlen (p) + 1;
      totsize += size;
      p += size;
    }
  return totsize + 1;  
}



static FILE *listed_incremental_stream;

/* Version of incremental format snapshots (directory files) used by this
   tar. Currently it is supposed to be a single decimal number. 0 means
   incremental snapshots as per tar version before 1.15.2.

   The current tar version supports incremental versions from
   0 up to TAR_INCREMENTAL_VERSION, inclusive.
   It is able to create only snapshots of TAR_INCREMENTAL_VERSION */

#define TAR_INCREMENTAL_VERSION 1

/* Read incremental snapshot file (directory file).
   If the file has older incremental version, make sure that it is processed
   correctly and that tar will use the most conservative backup method among
   possible alternatives (i.e. prefer ALL_CHILDREN over CHANGED_CHILDREN,
   etc.) This ensures that the snapshots are updated to the recent version
   without any loss of data. */ 
void
read_directory_file (void)
{
  int fd;
  FILE *fp;
  char *buf = 0;
  size_t bufsize;

  /* Open the file for both read and write.  That way, we can write
     it later without having to reopen it, and don't have to worry if
     we chdir in the meantime.  */
  fd = open (listed_incremental_option, O_RDWR | O_CREAT, MODE_RW);
  if (fd < 0)
    {
      open_error (listed_incremental_option);
      return;
    }

  fp = fdopen (fd, "r+");
  if (! fp)
    {
      open_error (listed_incremental_option);
      close (fd);
      return;
    }

  listed_incremental_stream = fp;

  if (0 < getline (&buf, &bufsize, fp))
    {
      char *ebuf;
      int n;
      long lineno = 1;
      uintmax_t u;
      time_t t = u;
      int incremental_version;
      
      if (strncmp (buf, PACKAGE_NAME, sizeof PACKAGE_NAME - 1) == 0)
	{
	  ebuf = buf + sizeof PACKAGE_NAME - 1;
	  if (*ebuf++ != '-')
	    ERROR((1, 0, _("Bad incremental file format")));
	  for (; *ebuf != '-'; ebuf++)
	    if (!*ebuf)
	      ERROR((1, 0, _("Bad incremental file format")));
	  
	  incremental_version = (errno = 0, strtoumax (ebuf+1, &ebuf, 10));
	  if (getline (&buf, &bufsize, fp) <= 0)
	    {
	      read_error (listed_incremental_option);
	      free (buf);
	      return;
	    }
	  ++lineno;
	}
      else
	incremental_version = 0;

      if (incremental_version > TAR_INCREMENTAL_VERSION)
	ERROR((1, 0, _("Unsupported incremental format version: %d"),
	       incremental_version));
      
      t = u = (errno = 0, strtoumax (buf, &ebuf, 10));
      if (buf == ebuf || (u == 0 && errno == EINVAL))
	ERROR ((0, 0, "%s:%ld: %s",
		quotearg_colon (listed_incremental_option),
		lineno,
		_("Invalid time stamp")));
      else if (t != u)
	ERROR ((0, 0, "%s:%ld: %s",
		quotearg_colon (listed_incremental_option),
		lineno,
		_("Time stamp out of range")));
      else if (incremental_version == 1)
	{
	  newer_mtime_option.tv_sec = t;
	  
	  t = u = (errno = 0, strtoumax (buf, &ebuf, 10));
	  if (buf == ebuf || (u == 0 && errno == EINVAL))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option),
		    lineno,
		    _("Invalid time stamp")));
	  else if (t != u)
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option),
		    lineno,
		    _("Time stamp out of range")));
	  newer_mtime_option.tv_nsec = t;
	}
      else
	{
	  /* pre-1 incremental format does not contain nanoseconds */
	  newer_mtime_option.tv_sec = t;
	  newer_mtime_option.tv_nsec = 0;
	}

      while (0 < (n = getline (&buf, &bufsize, fp)))
	{
	  dev_t dev;
	  ino_t ino;
	  bool nfs = buf[0] == '+';
	  char *strp = buf + nfs;
	  struct timespec mtime;

	  lineno++;

	  if (buf[n - 1] == '\n')
	    buf[n - 1] = '\0';

	  if (incremental_version == 1)
	    {
	      errno = 0;
	      mtime.tv_sec = u = strtoumax (strp, &ebuf, 10);
	      if (!isspace (*ebuf))
		ERROR ((0, 0, "%s:%ld: %s",
			quotearg_colon (listed_incremental_option), lineno,
			_("Invalid modification time (seconds)")));
	      else if (mtime.tv_sec != u) 
		ERROR ((0, 0, "%s:%ld: %s",
			quotearg_colon (listed_incremental_option), lineno,
			_("Modification time (seconds) out of range")));
	      strp = ebuf;
	  
	      errno = 0;
	      mtime.tv_nsec = u = strtoumax (strp, &ebuf, 10);
	      if (!isspace (*ebuf))
		ERROR ((0, 0, "%s:%ld: %s",
			quotearg_colon (listed_incremental_option), lineno,
			_("Invalid modification time (nanoseconds)")));
	      else if (mtime.tv_nsec != u) 
		ERROR ((0, 0, "%s:%ld: %s",
			quotearg_colon (listed_incremental_option), lineno,
			_("Modification time (nanoseconds) out of range")));
	      strp = ebuf;
	    }
	  else
	    memset (&mtime, 0, sizeof mtime);
	  
	  errno = 0;
	  dev = u = strtoumax (strp, &ebuf, 10);
	  if (!isspace (*ebuf))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Invalid device number")));
	  else if (dev != u) 
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Device number out of range")));
	  strp = ebuf;

	  errno = 0;
	  ino = u = strtoumax (strp, &ebuf, 10);
	  if (!isspace (*ebuf))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Invalid inode number")));
	  else if (ino != u)
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Inode number out of range")));
	  strp = ebuf;

	  strp++;
	  unquote_string (strp);
	  note_directory (strp, mtime, dev, ino, nfs, 0);
	}
    }

  if (ferror (fp))
    read_error (listed_incremental_option);
  if (buf)
    free (buf);
}

/* Output incremental data for the directory ENTRY to the file DATA.
   Return nonzero if successful, preserving errno on write failure.  */
static bool
write_directory_file_entry (void *entry, void *data)
{
  struct directory const *directory = entry;
  FILE *fp = data;

  if (directory->found)
    {
      int e;
      char buf[UINTMAX_STRSIZE_BOUND];
      char *str = quote_copy_string (directory->name);
      
      if (directory->nfs)
	fprintf (fp, "+");
      fprintf (fp, "%s ", umaxtostr (directory->mtime.tv_sec, buf));
      fprintf (fp, "%s ", umaxtostr (directory->mtime.tv_nsec, buf));
      fprintf (fp, "%s ", umaxtostr (directory->device_number, buf));
      fprintf (fp, "%s ", umaxtostr (directory->inode_number, buf));
      fprintf (fp, "%s\n", str ? str : directory->name);
	       
      e = errno;
      if (str)
	free (str);
      errno = e;
    }

  return ! ferror (fp);
}

void
write_directory_file (void)
{
  FILE *fp = listed_incremental_stream;

  if (! fp)
    return;

  if (fseek (fp, 0L, SEEK_SET) != 0)
    seek_error (listed_incremental_option);
  if (sys_truncate (fileno (fp)) != 0)
    truncate_error (listed_incremental_option);

  fprintf (fp, "%s-%s-%d\n", PACKAGE_NAME, PACKAGE_VERSION,
	   TAR_INCREMENTAL_VERSION);
  
  fprintf (fp, "%lu %lu\n",
	   (unsigned long int) start_time.tv_sec,
	   (unsigned long int) start_time.tv_nsec);
  if (! ferror (fp) && directory_table)
    hash_do_for_each (directory_table, write_directory_file_entry, fp);
  if (ferror (fp))
    write_error (listed_incremental_option);
  if (fclose (fp) != 0)
    close_error (listed_incremental_option);
}


/* Restoration of incremental dumps.  */

void
get_gnu_dumpdir ()
{
  size_t size;
  size_t copied;
  union block *data_block;
  char *to;
  char *archive_dir;
  
  size = current_stat_info.stat.st_size;
  if (size != current_stat_info.stat.st_size)
    xalloc_die ();

  archive_dir = xmalloc (size);
  to = archive_dir;

  set_next_block_after (current_header);
  mv_begin (&current_stat_info);

  for (; size > 0; size -= copied)
    {
      mv_size_left (size);
      data_block = find_next_block ();
      if (!data_block)
	ERROR ((1, 0, _("Unexpected EOF in archive")));
      copied = available_space_after (data_block);
      if (copied > size)
	copied = size;
      memcpy (to, data_block->buffer, copied);
      to += copied;
      set_next_block_after ((union block *)
			    (data_block->buffer + copied - 1));
    }

  mv_end ();
  
  current_stat_info.stat.st_size = 0; /* For skip_member() and friends
					 to work correctly */
  current_stat_info.dumpdir = archive_dir;
}


/* Examine the directories under directory_name and delete any
   files that were not there at the time of the back-up. */
void
purge_directory (char const *directory_name)
{
  char *current_dir;
  char *cur, *arc;

  if (!current_stat_info.dumpdir)
    {
      skip_member ();
      return;
    }
  
  current_dir = savedir (directory_name);

  if (!current_dir)
    {
      /* The directory doesn't exist now.  It'll be created.  In any
	 case, we don't have to delete any files out of it.  */

      skip_member ();
      return;
    }

  for (cur = current_dir; *cur; cur += strlen (cur) + 1)
    {
      for (arc = current_stat_info.dumpdir; *arc; arc += strlen (arc) + 1)
	{
	  arc++;
	  if (!strcmp (arc, cur))
	    break;
	}
      if (*arc == '\0')
	{
	  struct stat st;
	  char *p = new_name (directory_name, cur);

	  if (deref_stat (false, p, &st))
	    {
	      stat_diag (p);
	      WARN((0, 0, _("%s: Not purging directory: unable to stat"),
		    quotearg_colon (p)));
	      continue;
	    }
	  else if (one_file_system_option && st.st_dev != root_device)
	    {
	      WARN((0, 0,
		    _("%s: directory is on a different device: not purging"),
		    quotearg_colon (p)));
	      continue;
	    }

	  if (! interactive_option || confirm ("delete", p))
	    {
	      if (verbose_option)
		fprintf (stdlis, _("%s: Deleting %s\n"),
			 program_name, quote (p));
	      if (! remove_any_file (p, RECURSIVE_REMOVE_OPTION))
		{
		  int e = errno;
		  ERROR ((0, e, _("%s: Cannot remove"), quotearg_colon (p)));
		}
	    }
	  free (p);
	}

    }
  free (current_dir);
}

void
list_dumpdir (char *buffer, size_t size)
{
  while (size)
    {
      switch (*buffer)
	{
	case 'Y':
	case 'N':
	case 'D':
	  fprintf (stdlis, "%c ", *buffer);
	  buffer++;
	  size--;
	  break;
	  
	case 0:
	  fputc ('\n', stdlis);
	  buffer++;
	  size--;
	  break;
	  
	default:
	  fputc (*buffer, stdlis);
	  buffer++;
	  size--;
	}
    }
}

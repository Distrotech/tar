/* Common declarations for the tar program.

   Copyright (C) 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001,
   2003, 2004, 2005, 2006 Free Software Foundation, Inc.

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

/* Declare the GNU tar archive format.  */
#include "tar.h"

/* The checksum field is filled with this while the checksum is computed.  */
#define CHKBLANKS	"        "	/* 8 blanks, no null */

/* Some constants from POSIX are given names.  */
#define NAME_FIELD_SIZE   100
#define PREFIX_FIELD_SIZE 155
#define UNAME_FIELD_SIZE   32
#define GNAME_FIELD_SIZE   32



/* Some various global definitions.  */

/* Name of file to use for interacting with user.  */

/* GLOBAL is defined to empty in tar.c only, and left alone in other *.c
   modules.  Here, we merely set it to "extern" if it is not already set.
   GNU tar does depend on the system loader to preset all GLOBAL variables to
   neutral (or zero) values, explicit initialization is usually not done.  */
#ifndef GLOBAL
# define GLOBAL extern
#endif

#define TAREXIT_SUCCESS PAXEXIT_SUCCESS
#define TAREXIT_DIFFERS PAXEXIT_DIFFERS
#define TAREXIT_FAILURE PAXEXIT_FAILURE


#include "arith.h"
#include <backupfile.h>
#include <exclude.h>
#include <full-write.h>
#include <modechange.h>
#include <quote.h>
#include <safe-read.h>
#include <stat-time.h>
#include <timespec.h>
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
#include <obstack.h>

#include <paxlib.h>

/* Log base 2 of common values.  */
#define LG_8 3
#define LG_64 6
#define LG_256 8

/* Information gleaned from the command line.  */

/* Name of this program.  */
GLOBAL const char *program_name;

/* Main command option.  */

enum subcommand
{
  UNKNOWN_SUBCOMMAND,		/* none of the following */
  APPEND_SUBCOMMAND,		/* -r */
  CAT_SUBCOMMAND,		/* -A */
  CREATE_SUBCOMMAND,		/* -c */
  DELETE_SUBCOMMAND,		/* -D */
  DIFF_SUBCOMMAND,		/* -d */
  EXTRACT_SUBCOMMAND,		/* -x */
  LIST_SUBCOMMAND,		/* -t */
  UPDATE_SUBCOMMAND		/* -u */
};

GLOBAL enum subcommand subcommand_option;

/* Selected format for output archive.  */
GLOBAL enum archive_format archive_format;

/* Either NL or NUL, as decided by the --null option.  */
GLOBAL char filename_terminator;

/* Size of each record, once in blocks, once in bytes.  Those two variables
   are always related, the second being BLOCKSIZE times the first.  They do
   not have _option in their name, even if their values is derived from
   option decoding, as these are especially important in tar.  */
GLOBAL int blocking_factor;
GLOBAL size_t record_size;

GLOBAL bool absolute_names_option;

/* Display file times in UTC */
GLOBAL bool utc_option;

/* This variable tells how to interpret newer_mtime_option, below.  If zero,
   files get archived if their mtime is not less than newer_mtime_option.
   If nonzero, files get archived if *either* their ctime or mtime is not less
   than newer_mtime_option.  */
GLOBAL int after_date_option;

enum atime_preserve
{
  no_atime_preserve,
  replace_atime_preserve,
  system_atime_preserve
};
GLOBAL enum atime_preserve atime_preserve_option;

GLOBAL bool backup_option;

/* Type of backups being made.  */
GLOBAL enum backup_type backup_type;

GLOBAL bool block_number_option;

GLOBAL bool checkpoint_option;

/* Specified name of compression program, or "gzip" as implied by -z.  */
GLOBAL const char *use_compress_program_option;

GLOBAL bool dereference_option;

/* Print a message if not all links are dumped */
GLOBAL int check_links_option;

/* Patterns that match file names to be excluded.  */
GLOBAL struct exclude *excluded;

/* Exclude directories containing a cache directory tag. */
GLOBAL bool exclude_caches_option;

/* Specified value to be put into tar file in place of stat () results, or
   just -1 if such an override should not take place.  */
GLOBAL gid_t group_option;

GLOBAL bool ignore_failed_read_option;

GLOBAL bool ignore_zeros_option;

GLOBAL bool incremental_option;

/* Specified name of script to run at end of each tape change.  */
GLOBAL const char *info_script_option;

GLOBAL bool interactive_option;

/* If nonzero, extract only Nth occurrence of each named file */
GLOBAL uintmax_t occurrence_option;

enum old_files
{
  DEFAULT_OLD_FILES,          /* default */
  NO_OVERWRITE_DIR_OLD_FILES, /* --no-overwrite-dir */
  OVERWRITE_OLD_FILES,        /* --overwrite */
  UNLINK_FIRST_OLD_FILES,     /* --unlink-first */
  KEEP_OLD_FILES,             /* --keep-old-files */
  KEEP_NEWER_FILES	      /* --keep-newer-files */
};
GLOBAL enum old_files old_files_option;

/* Specified file name for incremental list.  */
GLOBAL const char *listed_incremental_option;

/* Specified mode change string.  */
GLOBAL struct mode_change *mode_option;

/* Initial umask, if needed for mode change string.  */
GLOBAL mode_t initial_umask;

GLOBAL bool multi_volume_option;

/* Specified threshold date and time.  Files having an older time stamp
   do not get archived (also see after_date_option above).  */
GLOBAL struct timespec newer_mtime_option;

/* Return true if newer_mtime_option is initialized.  */
#define NEWER_OPTION_INITIALIZED(opt) (0 <= (opt).tv_nsec)

/* Return true if the struct stat ST's M time is less than
   newer_mtime_option.  */
#define OLDER_STAT_TIME(st, m) \
  (timespec_cmp (get_stat_##m##time (&(st)), newer_mtime_option) < 0)

/* Likewise, for struct tar_stat_info ST.  */
#define OLDER_TAR_STAT_TIME(st, m) \
  (timespec_cmp ((st).m##time, newer_mtime_option) < 0)

/* Zero if there is no recursion, otherwise FNM_LEADING_DIR.  */
GLOBAL int recursion_option;

GLOBAL bool numeric_owner_option;

GLOBAL bool one_file_system_option;

/* Specified value to be put into tar file in place of stat () results, or
   just -1 if such an override should not take place.  */
GLOBAL uid_t owner_option;

GLOBAL bool recursive_unlink_option;

GLOBAL bool read_full_records_option;

GLOBAL bool remove_files_option;

/* Specified rmt command.  */
GLOBAL const char *rmt_command_option;

/* Specified remote shell command.  */
GLOBAL const char *rsh_command_option;

GLOBAL bool same_order_option;

/* If positive, preserve ownership when extracting.  */
GLOBAL int same_owner_option;

/* If positive, preserve permissions when extracting.  */
GLOBAL int same_permissions_option;

/* When set, strip the given number of file name components from the file name
   before extracting */
GLOBAL size_t strip_name_components;

GLOBAL bool show_omitted_dirs_option;

GLOBAL bool sparse_option;

GLOBAL bool starting_file_option;

/* Specified maximum byte length of each tape volume (multiple of 1024).  */
GLOBAL tarlong tape_length_option;

GLOBAL bool to_stdout_option;

GLOBAL bool totals_option;

GLOBAL bool touch_option;

GLOBAL char *to_command_option;
GLOBAL bool ignore_command_error_option;

/* Restrict some potentially harmful tar options */
GLOBAL bool restrict_option;

/* Return true if the extracted files are not being written to disk */
#define EXTRACT_OVER_PIPE (to_stdout_option || to_command_option)

/* Count how many times the option has been set, multiple setting yields
   more verbose behavior.  Value 0 means no verbosity, 1 means file name
   only, 2 means file name and all attributes.  More than 2 is just like 2.  */
GLOBAL int verbose_option;

GLOBAL bool verify_option;

/* Specified name of file containing the volume number.  */
GLOBAL const char *volno_file_option;

/* Specified value or pattern.  */
GLOBAL const char *volume_label_option;

/* Other global variables.  */

/* File descriptor for archive file.  */
GLOBAL int archive;

/* Nonzero when outputting to /dev/null.  */
GLOBAL bool dev_null_output;

/* Timestamp for when we started execution.  */
GLOBAL struct timespec start_time;

GLOBAL struct tar_stat_info current_stat_info;

/* List of tape drive names, number of such tape drives, allocated number,
   and current cursor in list.  */
GLOBAL const char **archive_name_array;
GLOBAL size_t archive_names;
GLOBAL size_t allocated_archive_names;
GLOBAL const char **archive_name_cursor;

/* Output index file name.  */
GLOBAL char const *index_file_name;

/* Structure for keeping track of filenames and lists thereof.  */
struct name
  {
    struct name *next;          /* Link to the next element */
    int change_dir;		/* Number of the directory to change to.
				   Set with the -C option. */
    uintmax_t found_count;	/* number of times a matching file has
				   been found */
    int explicit;               /* this name was explicitely given in the
				   command line */
    int matching_flags;		/* this name is a regexp, not literal */
    char const *dir_contents;	/* for incremental_option */

    size_t length;		/* cached strlen(name) */
    char name[1];
  };

/* Obnoxious test to see if dimwit is trying to dump the archive.  */
GLOBAL dev_t ar_dev;
GLOBAL ino_t ar_ino;

GLOBAL bool seekable_archive;

GLOBAL dev_t root_device;

/* Unquote filenames */
GLOBAL bool unquote_option;

GLOBAL bool test_label_option; /* Test archive volume label and exit */

/* When creating archive in verbose mode, list member names as stored in the
   archive */
GLOBAL bool show_stored_names_option;

/* Delay setting modification times and permissions of extracted directories
   until the end of extraction. This variable helps correctly restore directory
   timestamps from archives with an unusual member order. It is automatically
   set for incremental archives. */
GLOBAL bool delay_directory_restore_option;

/* Warn about implicit use of the wildcards in command line arguments.
   (Default for tar prior to 1.15.91, but changed afterwards */
GLOBAL bool warn_regex_usage;

/* Declarations for each module.  */

/* FIXME: compare.c should not directly handle the following variable,
   instead, this should be done in buffer.c only.  */

enum access_mode
{
  ACCESS_READ,
  ACCESS_WRITE,
  ACCESS_UPDATE
};
extern enum access_mode access_mode;

/* Module buffer.c.  */

extern FILE *stdlis;
extern bool write_archive_to_stdout;
extern char *volume_label;
extern char *continued_file_name;
extern uintmax_t continued_file_size;
extern uintmax_t continued_file_offset;

size_t available_space_after (union block *);
off_t current_block_ordinal (void);
void close_archive (void);
void closeout_volume_number (void);
union block *find_next_block (void);
void flush_read (void);
void flush_write (void);
void flush_archive (void);
void init_volume_number (void);
void open_archive (enum access_mode);
void print_total_written (void);
void reset_eof (void);
void set_next_block_after (union block *);
void clear_read_error_count (void);
void xclose (int fd);
void archive_write_error (ssize_t) __attribute__ ((noreturn));
void archive_read_error (void);
off_t seek_archive (off_t size);
void set_start_time (void);

void mv_begin (struct tar_stat_info *st);
void mv_end (void);
void mv_total_size (off_t size);
void mv_size_left (off_t size);


/* Module create.c.  */

enum dump_status
  {
    dump_status_ok,
    dump_status_short,
    dump_status_fail,
    dump_status_not_implemented
  };

bool file_dumpable_p (struct tar_stat_info *);
void create_archive (void);
void pad_archive (off_t size_left);
void dump_file (const char *, int, dev_t);
union block *start_header (struct tar_stat_info *st);
void finish_header (struct tar_stat_info *, union block *, off_t);
void simple_finish_header (union block *header);
union block * write_extended (bool global, struct tar_stat_info *st,
			      union block *old_header);
union block *start_private_header (const char *name, size_t size);
void write_eot (void);
void check_links (void);

#define GID_TO_CHARS(val, where) gid_to_chars (val, where, sizeof (where))
#define MAJOR_TO_CHARS(val, where) major_to_chars (val, where, sizeof (where))
#define MINOR_TO_CHARS(val, where) minor_to_chars (val, where, sizeof (where))
#define MODE_TO_CHARS(val, where) mode_to_chars (val, where, sizeof (where))
#define OFF_TO_CHARS(val, where) off_to_chars (val, where, sizeof (where))
#define SIZE_TO_CHARS(val, where) size_to_chars (val, where, sizeof (where))
#define TIME_TO_CHARS(val, where) time_to_chars (val, where, sizeof (where))
#define UID_TO_CHARS(val, where) uid_to_chars (val, where, sizeof (where))
#define UINTMAX_TO_CHARS(val, where) uintmax_to_chars (val, where, sizeof (where))
#define UNAME_TO_CHARS(name,buf) string_to_chars (name, buf, sizeof(buf))
#define GNAME_TO_CHARS(name,buf) string_to_chars (name, buf, sizeof(buf))

bool gid_to_chars (gid_t, char *, size_t);
bool major_to_chars (major_t, char *, size_t);
bool minor_to_chars (minor_t, char *, size_t);
bool mode_to_chars (mode_t, char *, size_t);
bool off_to_chars (off_t, char *, size_t);
bool size_to_chars (size_t, char *, size_t);
bool time_to_chars (time_t, char *, size_t);
bool uid_to_chars (uid_t, char *, size_t);
bool uintmax_to_chars (uintmax_t, char *, size_t);
void string_to_chars (char const *, char *, size_t);

/* Module diffarch.c.  */

extern bool now_verifying;

void diff_archive (void);
void diff_init (void);
void verify_volume (void);

/* Module extract.c.  */

void extr_init (void);
void extract_archive (void);
void extract_finish (void);
bool rename_directory (char *src, char *dst);

/* Module delete.c.  */

void delete_archive_members (void);

/* Module incremen.c.  */

char *get_directory_contents (char *, dev_t);
const char *append_incremental_renames (const char *dump);
void read_directory_file (void);
void write_directory_file (void);
void purge_directory (char const *);
void list_dumpdir (char *buffer, size_t size);
void update_parent_directory (const char *name);

size_t dumpdir_size (const char *p);
bool is_dumpdir (struct tar_stat_info *stat_info);

/* Module list.c.  */

enum read_header
{
  HEADER_STILL_UNREAD,		/* for when read_header has not been called */
  HEADER_SUCCESS,		/* header successfully read and checksummed */
  HEADER_SUCCESS_EXTENDED,	/* likewise, but we got an extended header */
  HEADER_ZERO_BLOCK,		/* zero block where header expected */
  HEADER_END_OF_FILE,		/* true end of file while header expected */
  HEADER_FAILURE		/* ill-formed header, or bad checksum */
};

struct xheader
{
  struct obstack *stk;
  size_t size;
  char *buffer;
};

GLOBAL struct xheader extended_header;
extern union block *current_header;
extern enum archive_format current_format;
extern size_t recent_long_name_blocks;
extern size_t recent_long_link_blocks;

void decode_header (union block *, struct tar_stat_info *,
		    enum archive_format *, int);
char const *tartime (struct timespec, bool);

#define GID_FROM_HEADER(where) gid_from_header (where, sizeof (where))
#define MAJOR_FROM_HEADER(where) major_from_header (where, sizeof (where))
#define MINOR_FROM_HEADER(where) minor_from_header (where, sizeof (where))
#define MODE_FROM_HEADER(where) mode_from_header (where, sizeof (where))
#define OFF_FROM_HEADER(where) off_from_header (where, sizeof (where))
#define SIZE_FROM_HEADER(where) size_from_header (where, sizeof (where))
#define TIME_FROM_HEADER(where) time_from_header (where, sizeof (where))
#define UID_FROM_HEADER(where) uid_from_header (where, sizeof (where))
#define UINTMAX_FROM_HEADER(where) uintmax_from_header (where, sizeof (where))

gid_t gid_from_header (const char *, size_t);
major_t major_from_header (const char *, size_t);
minor_t minor_from_header (const char *, size_t);
mode_t mode_from_header (const char *, size_t);
off_t off_from_header (const char *, size_t);
size_t size_from_header (const char *, size_t);
time_t time_from_header (const char *, size_t);
uid_t uid_from_header (const char *, size_t);
uintmax_t uintmax_from_header (const char *, size_t);

void list_archive (void);
void print_for_mkdir (char *, int, mode_t);
void print_header (struct tar_stat_info *, off_t);
void read_and (void (*) (void));
enum read_header read_header_primitive (bool raw_extended_headers,
					struct tar_stat_info *info);
enum read_header read_header (bool);
enum read_header tar_checksum (union block *header, bool silent);
void skip_file (off_t);
void skip_member (void);

/* Module mangle.c.  */

void extract_mangle (void);

/* Module misc.c.  */

void assign_string (char **, const char *);
char *quote_copy_string (const char *);
int unquote_string (char *);

void code_ns_fraction (int, char *);
char const *code_timespec (struct timespec, char *);
enum { BILLION = 1000000000, LOG10_BILLION = 9 };
enum { TIMESPEC_STRSIZE_BOUND =
         UINTMAX_STRSIZE_BOUND + LOG10_BILLION + sizeof "-." - 1 };

size_t dot_dot_prefix_len (char const *);

enum remove_option
{
  ORDINARY_REMOVE_OPTION,
  RECURSIVE_REMOVE_OPTION,

  /* FIXME: The following value is never used. It seems to be intended
     as a placeholder for a hypothetical option that should instruct tar
     to recursively remove subdirectories in purge_directory(),
     as opposed to the functionality of --recursive-unlink
     (RECURSIVE_REMOVE_OPTION value), which removes them in
     prepare_to_extract() phase. However, with the addition of more
     meta-info to the incremental dumps, this should become unnecessary */
  WANT_DIRECTORY_REMOVE_OPTION
};
int remove_any_file (const char *, enum remove_option);
bool maybe_backup_file (const char *, int);
void undo_last_backup (void);

int deref_stat (bool, char const *, struct stat *);

int chdir_arg (char const *);
void chdir_do (int);

void close_diag (char const *name);
void open_diag (char const *name);
void read_diag_details (char const *name, off_t offset, size_t size);
void readlink_diag (char const *name);
void savedir_diag (char const *name);
void seek_diag_details (char const *, off_t);
void stat_diag (char const *name);
void write_error_details (char const *, size_t, size_t);
void write_fatal (char const *) __attribute__ ((noreturn));
void write_fatal_details (char const *, ssize_t, size_t)
     __attribute__ ((noreturn));

pid_t xfork (void);
void xpipe (int[2]);

void *page_aligned_alloc (void **, size_t);
int set_file_atime (int fd, char const *file,
		    struct timespec const timespec[2]);

/* Module names.c.  */

extern struct name *gnu_list_name;

void gid_to_gname (gid_t, char **gname);
int gname_to_gid (char const *, gid_t *);
void uid_to_uname (uid_t, char **uname);
int uname_to_uid (char const *, uid_t *);

void init_names (void);
void name_add_name (const char *, int);
void name_add_dir (const char *name);
void name_term (void);
const char *name_next (int);
void name_gather (void);
struct name *addname (char const *, int);
int name_match (const char *);
void names_notfound (void);
void collect_and_sort_names (void);
struct name *name_scan (const char *, bool);
char *name_from_list (void);
void blank_name_list (void);
char *new_name (const char *, const char *);
size_t stripped_prefix_len (char const *file_name, size_t num);
bool all_names_found (struct tar_stat_info *);

bool excluded_name (char const *);

void add_avoided_name (char const *);
bool is_avoided_name (char const *);
bool is_individual_file (char const *);

bool contains_dot_dot (char const *);

#define ISFOUND(c) ((occurrence_option == 0) ? (c)->found_count : \
                    (c)->found_count == occurrence_option)
#define WASFOUND(c) ((occurrence_option == 0) ? (c)->found_count : \
                     (c)->found_count >= occurrence_option)

/* Module tar.c.  */

void usage (int);

int confirm (const char *, const char *);
void request_stdin (const char *);

void tar_stat_init (struct tar_stat_info *st);
void tar_stat_destroy (struct tar_stat_info *st);
void usage (int) __attribute__ ((noreturn));
int tar_timespec_cmp (struct timespec a, struct timespec b);
const char *archive_format_string (enum archive_format fmt);
const char *subcommand_string (enum subcommand c);

/* Module update.c.  */

extern char *output_start;

void update_archive (void);

/* Module xheader.c.  */

void xheader_decode (struct tar_stat_info *);
void xheader_decode_global (void);
void xheader_store (char const *, struct tar_stat_info const *, void const *);
void xheader_read (union block *, size_t);
void xheader_write (char type, char *name, struct xheader *xhdr);
void xheader_write_global (void);
void xheader_finish (struct xheader *);
void xheader_destroy (struct xheader *);
char *xheader_xhdr_name (struct tar_stat_info *st);
char *xheader_ghdr_name (void);
void xheader_write (char, char *, struct xheader *);
void xheader_write_global (void);
void xheader_set_option (char *string);
void xheader_string_begin (void);
void xheader_string_add (char const *s);
void xheader_string_end (char const *keyword);
bool xheader_keyword_deleted_p (const char *kw);
char *xheader_format_name (struct tar_stat_info *st, const char *fmt,
			   size_t n);

/* Module system.c */

void sys_detect_dev_null_output (void);
void sys_save_archive_dev_ino (void);
void sys_drain_input_pipe (void);
void sys_wait_for_child (pid_t);
void sys_spawn_shell (void);
bool sys_compare_uid (struct stat *a, struct stat *b);
bool sys_compare_gid (struct stat *a, struct stat *b);
bool sys_file_is_archive (struct tar_stat_info *p);
bool sys_compare_links (struct stat *link_data, struct stat *stat_data);
int sys_truncate (int fd);
pid_t sys_child_open_for_compress (void);
pid_t sys_child_open_for_uncompress (void);
size_t sys_write_archive_buffer (void);
bool sys_get_archive_stat (void);
int sys_exec_command (char *file_name, int typechar, struct tar_stat_info *st);
void sys_wait_command (void);
int sys_exec_info_script (const char **archive_name, int volume_number);

/* Module compare.c */
void report_difference (struct tar_stat_info *st, const char *message, ...);

/* Module sparse.c */
bool sparse_file_p (struct tar_stat_info *);
bool sparse_member_p (struct tar_stat_info *);
bool sparse_fixup_header (struct tar_stat_info *);
enum dump_status sparse_dump_file (int, struct tar_stat_info *);
enum dump_status sparse_extract_file (int, struct tar_stat_info *, off_t *);
enum dump_status sparse_skip_file (struct tar_stat_info *);
bool sparse_diff_file (int, struct tar_stat_info *);

/* Module utf8.c */
bool string_ascii_p (const char *str);
bool utf8_convert (bool to_utf, char const *input, char **output);

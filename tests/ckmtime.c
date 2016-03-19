/* Check if filesystem timestamps are consistent with the system time.
   Copyright (C) 2016 Free Software Foundation, Inc.
      
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <config.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stat-time.h>
#include <timespec.h>

int
main (int argc, char **argv)
{
  FILE *fp;
  struct timeval tv;
  struct stat st;
  struct timespec ts;
  
  assert (gettimeofday (&tv, NULL) == 0);
  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = tv.tv_usec * 1000;
  
  fp = tmpfile ();
  assert (fp != NULL);
  assert (fstat (fileno (fp), &st) == 0);
  fclose (fp);
  if (timespec_cmp (get_stat_mtime (&st), ts) >= 0)
    {
      fprintf (stderr, "file timestamp unreliable\n");
      return 1;
    }
  return 0;
}

/* Read files directly from the fast file system
   Copyright (C) 1992 Free Software Foundation 

   This file is part of GNU Tar.

   GNU Tar is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   GNU Tar is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Tar; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */


dev_t lastdev;
ino_t lastino;

struct dinode ino;
struct fs fs;
off_t offset;
int device_fd;

int *sindir, *dindir, *tindir;
int sindirblk, dindirblk, tindirblk;

read_raw_file (fd, buf, len)
     int fd;
     char *buf;
     int len;
{
  struct stat st;
  off_t ntoread;
  int log_blkno, phys_blkno;
  
  fstat (fd, &st);
  if (st.st_dev != lastdev)
    new_device (st.st_dev);
  
  if (st.st_ino != lastino)
    new_inode (st.st_ino);

  /* Only read single blocks at a time */
  if (len > fs.fs_bsize)
    len = fs.fs_bsize;
  
  /* Prune to the length of the file */
  if (offset + len > ino.di_size)
    len = ino.di_size - offset;
  
  log_blkno = lblkno (&fs, blkno);
  

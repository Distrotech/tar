/* Run program with its first three file descriptors attached to a tty.

   Copyright 2014 Free Software Foundation, Inc.

   This file is part of GNU tar.

   GNU tar is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   GNU tar is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define _XOPEN_SOURCE 600
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <termios.h>
#include <sys/ioctl.h>

#ifndef TCSASOFT
# define TCSASOFT 0
#endif

#define C_EOT 4

#define EX_OK 0
#define EX_USAGE 125
#define EX_ERR   126
#define EX_EXEC  127

#define BUF_SIZE 1024

#if 0
# define DEBUG(c) fprintf (stderr, "%s\n", c)
#else
# define DEBUG(c)
#endif

struct buffer
{
  char buf[BUF_SIZE];
  int avail;
  int written;
  int cr;
  time_t ts;
};

#define shut(fildes)						\
  do								\
    {								\
      DEBUG (("closing " #fildes));				\
      close(fildes);						\
      fildes = -1;						\
    }								\
  while(0)

#define bufinit(buffer,all)				\
  do							\
    {							\
      (buffer).avail = (buffer).written = 0;		\
      (buffer).ts = time (NULL);			\
      if (all)						\
	(buffer).cr = 0;				\
    }							\
  while(0)

#define bufisempty(buffer) ((buffer).avail == (buffer).written)
#define bufavail(buffer) (BUF_SIZE - (buffer).avail)

#define bufread(buffer,fildes,tty)					\
  do									\
    {									\
      int r = read (fildes, (buffer).buf + (buffer).avail,		\
		    BUF_SIZE - (buffer).avail);				\
      (buffer).ts = time (NULL);					\
      if (r < 0)							\
	{								\
	  if (errno == EINTR)						\
	    continue;							\
	  if (tty && errno == EIO)					\
	    shut (fildes);						\
	  else								\
	    {								\
	      fprintf (stderr, "%s:%d: reading from %s: %s",		\
		       __FILE__,__LINE__,#fildes, strerror (errno));	\
	      exit (EX_ERR);						\
	    }								\
	}								\
      else if (r == 0)							\
	shut (fildes);							\
      else								\
	(buffer).avail += r;						\
    }									\
  while(0)

#define bufwrite(buffer,fildes)						\
  do									\
    {									\
      int r = write (fildes, (buffer).buf + (buffer).written,		\
		     (buffer).avail - (buffer).written);		\
      (buffer).ts = time (NULL);					\
      if (r < 0)							\
	{								\
	  if (errno == EINTR)						\
	    continue;							\
	  if (stop)							\
	    shut (fildes);						\
	  else								\
	    {								\
	      perror ("writing");					\
	      exit (EX_ERR);						\
	    }								\
	}								\
      else if (r == 0)							\
	/*shut (fildes)*/;						\
      else								\
	(buffer).written += r;						\
    }									\
  while(0)

void
tr (struct buffer *bp)
{
  int i, j;

  for (i = j = bp->written; i < bp->avail;)
    {
      if (bp->buf[i] == '\r')
	{
	  bp->cr = 1;
	  i++;
	}
      else
	{
	  if (bp->cr)
	    {
	      bp->cr = 0;
	      if (bp->buf[i] != '\n')
		bp->buf[j++] = '\r';
	    }
	  bp->buf[j++] = bp->buf[i++];
	}
    }
  bp->avail = j;
}

int stop;
int status;

void
sigchld (int sig)
{
  DEBUG (("child exited"));
  wait (&status);
  stop = 1;
}

void
noecho (int fd)
{
  struct termios to;

  if (tcgetattr (fd, &to))
    {
      perror ("tcgetattr");
      exit (EX_ERR);
    }
  to.c_lflag |= ICANON;
  to.c_lflag &= ~(ECHO | ISIG);
  to.c_cc[VEOF] = C_EOT;
  if (tcsetattr (fd, TCSAFLUSH | TCSASOFT, &to))
    {
      perror ("tcsetattr");
      exit (EX_ERR);
    }
}

char *usage_text[] = {
  "usage: ttyemu [-ah] [-i INFILE] [-o OUTFILE] [-t TIMEOUT] PROGRAM [ARGS...]",
  "ttyemu runs PROGRAM with its first three file descriptors connected to a"
  " terminal",
  "",
  "Options are:",
  "",
  "   -a            append output to OUTFILE, instead of overwriting it",
  "   -i INFILE     read input from INFILE",
  "   -o OUTFILE    write output to OUTFILE",
  "   -t TIMEOUT    set I/O timeout",
  "   -h            print this help summary",
  "",
  "Report bugs and suggestions to <bug-tar@gnu.org>.",
  NULL
};

static void
usage (void)
{
  int i;
  
  for (i = 0; usage_text[i]; i++)
    {
      fputs (usage_text[i], stderr);
      fputc ('\n', stderr);
    }
}

int
main (int argc, char **argv)
{
  int i;
  int master, slave;
  pid_t pid;
  fd_set rdset, wrset;
  struct buffer ibuf, obuf;
  int in = 0, out = 1;
  char *infile = NULL, *outfile = NULL;
  int outflags = O_TRUNC;
  int maxfd;
  int eot = C_EOT;
  int timeout = 0;
  
  while ((i = getopt (argc, argv, "ai:o:t:h")) != EOF)
    {
      switch (i)
	{
	case 'a':
	  outflags &= ~O_TRUNC;
	  break;
	
	case 'i':
	  infile = optarg;
	  break;
	  
	case 'o':
	  outfile = optarg;
	  break;

	case 't':
	  timeout = atoi (optarg);
	  break;
	  
	case 'h':
	  usage ();
	  return EX_OK;
	  
	default:
	  return EX_USAGE;
	}
    }
  
  argc -= optind;
  argv += optind;

  if (argc == 0)
    {
      usage ();
      return EX_USAGE;
    }

  if (infile)
    {
      in = open (infile, O_RDONLY);
      if (in == -1)
	{
	  perror (infile);
	  return EX_ERR;
	}
    }

  if (outfile)
    {
      out = open (outfile, O_RDWR|O_CREAT|outflags, 0666);
      if (out == -1)
	{
	  perror (outfile);
	  return EX_ERR;
	}
    }
  
  master = posix_openpt (O_RDWR);
  if (master == -1)
    {
      perror ("posix_openpty");
      return EX_ERR;
    }

  if (grantpt (master))
    {
      perror ("grantpt");
      return EX_ERR;
    }

  if (unlockpt (master))
    {
      perror ("unlockpt");
      return EX_ERR;
    }
  
  signal (SIGCHLD, sigchld);

  pid = fork ();
  if (pid == -1)
    {
      perror ("fork");
      return EX_ERR;
    }

  if (pid == 0)
    {
      slave = open (ptsname (master), O_RDWR);
      if (slave < 0)
	{
	  perror ("open");
	  return EX_ERR;
	}

      noecho (slave);
      for (i = 0; i < 3; i++)
	{
	  if (slave != i)
	    {
	      close (i);
	      if (dup (slave) != i)
		{
		  perror ("dup");
		  _exit (EX_EXEC);
		}
	    }
	}
      for (i = sysconf (_SC_OPEN_MAX) - 1; i > 2; --i)
	close (i);

      setsid ();
#ifdef TIOCSCTTY
      ioctl (0, TIOCSCTTY, 1);
#endif      
      execvp (argv[0], argv);
      perror (argv[0]);
      _exit (EX_EXEC);
    }
  sleep (1);

  bufinit (ibuf, 1);
  bufinit (obuf, 1);
  while (1)
    {
      FD_ZERO (&rdset);
      FD_ZERO (&wrset);
      
      maxfd = 0;

      if (in != -1)
	{
	  FD_SET (in, &rdset);
	  if (in > maxfd)
	    maxfd = in;
	}

      if (master != -1)
	{
	  FD_SET (master, &rdset);
	  if (!stop)
	    FD_SET (master, &wrset);
	  if (master > maxfd)
	    maxfd = master;
	}

      if (maxfd == 0)
	{
	  if (stop)
	    break;
	  pause ();
	  continue;
	}
      
      if (select (maxfd + 1, &rdset, &wrset, NULL, NULL) < 0)
	{
	  if (errno == EINTR)
	    continue;
	  perror ("select");
	  return EX_ERR;
	}

      if (timeout)
	{
	  time_t now = time (NULL);
	  if (now - ibuf.ts > timeout || now - obuf.ts > timeout)
	    {
	      fprintf (stderr, "ttyemu: I/O timeout\n");
	      return EX_ERR;
	    }
	}
      
      if (in >= 0)
	{
	  if (bufavail (ibuf) && FD_ISSET (in, &rdset))
	    bufread (ibuf, in, 0);
	}
      else if (master == -1)
	break;

      if (master >= 0 && FD_ISSET (master, &wrset))
	{
	  if (!bufisempty (ibuf))
	    bufwrite (ibuf, master);
	  else if (in == -1 && eot)
	    {
	      DEBUG (("sent EOT"));
	      if (write (master, &eot, 1) <= 0)
		{
		  perror ("write");
		  return EX_ERR;
		}
	      eot = 0;
	    }
	}

      if (master >= 0 && bufavail (obuf) && FD_ISSET (master, &rdset))
	bufread (obuf, master, 1);

      if (bufisempty (obuf))
	bufinit (obuf, 0);
      else
	{
	  tr (&obuf);
	  bufwrite (obuf, out);
	}
      
      if (bufisempty (ibuf))
	bufinit (ibuf, 0);
    }

  if (WIFEXITED (status))
    return WEXITSTATUS (status);

  if (WIFSIGNALED (status))
    fprintf (stderr, "ttyemu: child process %s failed on signal %d\n",
	     argv[0], WTERMSIG (status));
  else
    fprintf (stderr, "ttyemu: child process %s failed\n", argv[0]);
  return EX_EXEC;
}

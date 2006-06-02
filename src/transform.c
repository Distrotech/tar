/* This file is part of GNU tar. 
   Copyright (C) 2006 Free Software Foundation, Inc.

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
#include <regex.h>
#include "common.h"

enum transform_type
  {
    transform_none,
    transform_first,
    transform_global
  }
transform_type = transform_none;
static regex_t regex;
static struct obstack stk;

enum replace_segm_type
  {
    segm_literal,   /* Literal segment */
    segm_backref,   /* Back-reference segment */
  };

struct replace_segm
{
  struct replace_segm *next;
  enum replace_segm_type type;
  union
  {
    struct
    {
      char *ptr;
      size_t size;
    } literal;
    size_t ref;
  } v;
};

static struct replace_segm *repl_head, *repl_tail;
static segm_count;

static struct replace_segm *
add_segment (void)
{
  struct replace_segm *segm = xmalloc (sizeof *segm);
  segm->next = NULL;
  if (repl_tail)
    repl_tail->next = segm;
  else
    repl_head = segm;
  repl_tail = segm;
  segm_count++;
  return segm;
}

static void
add_literal_segment (char *str, char *end)
{
  size_t len = end - str;
  if (len)
    {
      struct replace_segm *segm = add_segment ();
      segm->type = segm_literal;
      segm->v.literal.ptr = xmalloc (len + 1);
      memcpy (segm->v.literal.ptr, str, len);
      segm->v.literal.ptr[len] = 0;
      segm->v.literal.size = len;
    }
}

static void
add_char_segment (int chr)
{
  struct replace_segm *segm = add_segment ();
  segm->type = segm_literal;
  segm->v.literal.ptr = xmalloc (2);
  segm->v.literal.ptr[0] = chr;
  segm->v.literal.ptr[1] = 0;
  segm->v.literal.size = 2;
}

static void
add_backref_segment (size_t ref)
{
  struct replace_segm *segm = add_segment ();
  segm->type = segm_backref;
  segm->v.ref = ref;
}

void
set_transform_expr (const char *expr)
{
  int delim;
  int i, j, rc;
  char *str, *beg, *cur;
  const char *p;
  int cflags = 0;

  if (transform_type == transform_none)
    obstack_init (&stk);
  else
    {
      /* Redefinition of the transform expression */
      regfree (&regex);
    }

  if (expr[0] != 's')
    USAGE_ERROR ((0, 0, _("Invalid transform expression")));

  delim = expr[1];

  /* Scan regular expression */
  for (i = 2; expr[i] && expr[i] != delim; i++)
    if (expr[i] == '\\' && expr[i+1])
      i++;

  if (expr[i] != delim)
    USAGE_ERROR ((0, 0, _("Invalid transform expression")));

  /* Scan replacement expression */
  for (j = i + 1; expr[j] && expr[j] != delim; j++)
    if (expr[j] == '\\' && expr[j+1])
      j++;

  if (expr[j] != delim)
    USAGE_ERROR ((0, 0, _("Invalid transform expression")));

  /* Check flags */
  transform_type = transform_first;
  for (p = expr + j + 1; *p; p++)
    switch (*p)
      {
      case 'g':
	transform_type = transform_global;
	break;

      case 'i':
	cflags |= REG_ICASE;
	break;

      case 'x':
	cflags |= REG_EXTENDED;
	break;
	
      default:
	USAGE_ERROR ((0, 0, _("Unknown flag in transform expression")));
      }

  /* Extract and compile regex */
  str = xmalloc (i - 1);
  memcpy (str, expr + 2, i - 2);
  str[i - 2] = 0;

  rc = regcomp (&regex, str, cflags);
  
  if (rc)
    {
      char errbuf[512];
      regerror (rc, &regex, errbuf, sizeof (errbuf));
      USAGE_ERROR ((0, 0, _("Invalid transform expression: %s"), errbuf));
    }

  if (str[0] == '^' || str[strlen (str) - 1] == '$')
    transform_type = transform_first;
  
  free (str);

  /* Extract and compile replacement expr */
  i++;
  str = xmalloc (j - i + 1);
  memcpy (str, expr + i, j - i);
  str[j - i] = 0;

  for (cur = beg = str; *cur;)
    {
      if (*cur == '\\')
	{
	  size_t n;
	  
	  add_literal_segment (beg, cur);
	  switch (*++cur)
	    {
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	      n = strtoul (cur, &cur, 10);
	      if (n > regex.re_nsub)
		USAGE_ERROR ((0, 0, _("Invalid transform replacement: back reference out of range")));
	      add_backref_segment (n);
	      break;

	    case '\\':
	      add_char_segment ('\\');
	      cur++;
	      break;

	    case 'a':
	      add_char_segment ('\a');
	      cur++;
	      break;
	      
	    case 'b':
	      add_char_segment ('\b');
	      cur++;
	      break;
	      
	    case 'f':
	      add_char_segment ('\f');
	      cur++;
	      break;
	      
	    case 'n':
	      add_char_segment ('\n');
	      cur++;
	      break;
	      
	    case 'r':
	      add_char_segment ('\r');
	      cur++;
	      break;
	      
	    case 't':
	      add_char_segment ('\t');
	      cur++;
	      break;
	      
	    case 'v':
	      add_char_segment ('\v');
	      cur++;
	      break;

	    case '&':
	      add_char_segment ('&');
	      cur++;
	      break;

	    default:
	      /* Try to be nice */
	      {
		char buf[2];
		buf[0] = '\\';
		buf[1] = *cur;
		add_literal_segment (buf, buf + 2);
	      }
	      cur++;
	      break;
	    }
	  beg = cur;
	}
      else if (*cur == '&')
	{
	  add_literal_segment (beg, cur);
	  add_backref_segment (0);
	  beg = ++cur;
	}
      else
	cur++;
    }
  add_literal_segment (beg, cur);
  
}

bool
_transform_name_to_obstack (char *input)
{
  regmatch_t *rmp;
  char *p;
  int rc;
  
  if (transform_type == transform_none)
    return false;

  rmp = xmalloc ((regex.re_nsub + 1) * sizeof (*rmp));

  while (*input)
    {
      size_t disp;
      
      rc = regexec (&regex, input, regex.re_nsub + 1, rmp, 0);
      
      if (rc == 0)
	{
	  struct replace_segm *segm;

	  disp = rmp[0].rm_eo;

	  if (rmp[0].rm_so)
	    obstack_grow (&stk, input, rmp[0].rm_so);
	  
	  for (segm = repl_head; segm; segm = segm->next)
	    {
	      switch (segm->type)
		{
		case segm_literal:    /* Literal segment */
		  obstack_grow (&stk, segm->v.literal.ptr,
				segm->v.literal.size);
		  break;
	      
		case segm_backref:    /* Back-reference segment */
		  if (rmp[segm->v.ref].rm_so != -1
		      && rmp[segm->v.ref].rm_eo != -1)
		    obstack_grow (&stk,
				  input + rmp[segm->v.ref].rm_so,
			      rmp[segm->v.ref].rm_eo - rmp[segm->v.ref].rm_so);
		  break;
		}
	    }
	}
      else
	{
	  disp = strlen (input);
	  obstack_grow (&stk, input, disp);
	}

      input += disp;

      if (transform_type == transform_first)
	{
	  obstack_grow (&stk, input, strlen (input));
	  break;
	}
    }

  obstack_1grow (&stk, 0);
  free (rmp);
  return true;
}
  
bool
transform_name_fp (char **pinput, char *(*fun)(char *))
{
    char *str, *p;
    bool ret = _transform_name_to_obstack (*pinput);
    if (ret)
      {
	str = obstack_finish (&stk);
	assign_string (pinput, fun ? fun (str) : str);
	obstack_free (&stk, str);
      }
    return ret;
}

bool
transform_name (char **pinput)
{
  return transform_name_fp (pinput, NULL);
}

#if 0
void
read_and_transform_loop ()
{
  char buf[512];
  while (fgets (buf, sizeof buf, stdin))
    {
      char *p = buf + strlen (buf);
      if (p[-1] == '\n')
	p[-1] = 0;
      if (transform_name (buf, &p))
	printf ("=> %s\n", p);
      else
	printf ("=\n");
    }
}
#endif

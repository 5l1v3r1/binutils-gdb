/* Parse a printf-style format string.

   Copyright (C) 1986-2019 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "common-defs.h"
#include "format.h"

format_pieces::format_pieces (const char **arg, bool gdb_extensions)
{
  const char *s;
  char *f, *string;
  const char *prev_start;
  const char *percent_loc;
  char *sub_start, *current_substring;
  enum argclass this_argclass;

  s = *arg;

  /* Parse the format-control string and copy it into the string STRING,
     processing some kinds of escape sequence.  */

  f = string = (char *) alloca (strlen (s) + 1);

  while (*s != '"' && *s != '\0')
    {
      int c = *s++;
      switch (c)
	{
	case '\0':
	  continue;

	case '\\':
	  switch (c = *s++)
	    {
	    case '\\':
	      *f++ = '\\';
	      break;
	    case 'a':
	      *f++ = '\a';
	      break;
	    case 'b':
	      *f++ = '\b';
	      break;
	    case 'e':
	      *f++ = '\e';
	      break;
	    case 'f':
	      *f++ = '\f';
	      break;
	    case 'n':
	      *f++ = '\n';
	      break;
	    case 'r':
	      *f++ = '\r';
	      break;
	    case 't':
	      *f++ = '\t';
	      break;
	    case 'v':
	      *f++ = '\v';
	      break;
	    case '"':
	      *f++ = '"';
	      break;
	    default:
	      /* ??? TODO: handle other escape sequences.  */
	      error (_("Unrecognized escape character \\%c in format string."),
		     c);
	    }
	  break;

	default:
	  *f++ = c;
	}
    }

  /* Terminate our escape-processed copy.  */
  *f++ = '\0';

  /* Whether the format string ended with double-quote or zero, we're
     done with it; it's up to callers to complain about syntax.  */
  *arg = s;

  /* Need extra space for the '\0's.  Doubling the size is sufficient.  */

  current_substring = (char *) xmalloc (strlen (string) * 2 + 1000);
  m_storage.reset (current_substring);

  /* Now scan the string for %-specs and see what kinds of args they want.
     argclass classifies the %-specs so we can give printf-type functions
     something of the right size.  */

  f = string;
  prev_start = string;
  while (*f)
    if (*f++ == '%')
      {
	int seen_hash = 0, seen_zero = 0, lcount = 0, seen_prec = 0;
	int seen_space = 0, seen_plus = 0;
	int seen_big_l = 0, seen_h = 0, seen_big_h = 0;
	int seen_big_d = 0, seen_double_big_d = 0;
	int bad = 0;

	/* Skip over "%%", it will become part of a literal piece.  */
	if (*f == '%')
	  {
	    f++;
	    continue;
	  }

	sub_start = current_substring;

	strncpy (current_substring, prev_start, f - 1 - prev_start);
	current_substring += f - 1 - prev_start;
	*current_substring++ = '\0';

	m_pieces.emplace_back (sub_start, literal_piece);

	percent_loc = f - 1;

	/* Check the validity of the format specifier, and work
	   out what argument it expects.  We only accept C89
	   format strings, with the exception of long long (which
	   we autoconf for).  */

	/* The first part of a format specifier is a set of flag
	   characters.  */
	while (*f != '\0' && strchr ("0-+ #", *f))
	  {
	    if (*f == '#')
	      seen_hash = 1;
	    else if (*f == '0')
	      seen_zero = 1;
	    else if (*f == ' ')
	      seen_space = 1;
	    else if (*f == '+')
	      seen_plus = 1;
	    f++;
	  }

	/* The next part of a format specifier is a width.  */
	while (*f != '\0' && strchr ("0123456789", *f))
	  f++;

	/* The next part of a format specifier is a precision.  */
	if (*f == '.')
	  {
	    seen_prec = 1;
	    f++;
	    while (*f != '\0' && strchr ("0123456789", *f))
	      f++;
	  }

	/* The next part of a format specifier is a length modifier.  */
	if (*f == 'h')
	  {
	    seen_h = 1;
	    f++;
	  }
	else if (*f == 'l')
	  {
	    f++;
	    lcount++;
	    if (*f == 'l')
	      {
		f++;
		lcount++;
	      }
	  }
	else if (*f == 'L')
	  {
	    seen_big_l = 1;
	    f++;
	  }
	/* Decimal32 modifier.  */
	else if (*f == 'H')
	  {
	    seen_big_h = 1;
	    f++;
	  }
	/* Decimal64 and Decimal128 modifiers.  */
	else if (*f == 'D')
	  {
	    f++;

	    /* Check for a Decimal128.  */
	    if (*f == 'D')
	      {
		f++;
		seen_double_big_d = 1;
	      }
	    else
	      seen_big_d = 1;
	  }

	switch (*f)
	  {
	  case 'u':
	    if (seen_hash)
	      bad = 1;
	    /* FALLTHROUGH */

	  case 'o':
	  case 'x':
	  case 'X':
	    if (seen_space || seen_plus)
	      bad = 1;
	  /* FALLTHROUGH */

	  case 'd':
	  case 'i':
	    if (lcount == 0)
	      this_argclass = int_arg;
	    else if (lcount == 1)
	      this_argclass = long_arg;
	    else
	      this_argclass = long_long_arg;

	    if (seen_big_l)
	      bad = 1;
	    break;

	  case 'c':
	    this_argclass = lcount == 0 ? int_arg : wide_char_arg;
	    if (lcount > 1 || seen_h || seen_big_l)
	      bad = 1;
	    if (seen_prec || seen_zero || seen_space || seen_plus)
	      bad = 1;
	    break;

	  case 'p':
	    this_argclass = ptr_arg;
	    if (lcount || seen_h || seen_big_l)
	      bad = 1;
	    if (seen_prec)
	      bad = 1;
	    if (seen_hash || seen_zero || seen_space || seen_plus)
	      bad = 1;

	    if (gdb_extensions)
	      {
		switch (f[1])
		  {
		  case 's':
		  case 'S':
		  case 'F':
		    f++;
		    break;
		  }
	      }

	    break;

	  case 's':
	    this_argclass = lcount == 0 ? string_arg : wide_string_arg;
	    if (lcount > 1 || seen_h || seen_big_l)
	      bad = 1;
	    if (seen_zero || seen_space || seen_plus)
	      bad = 1;
	    break;

	  case 'e':
	  case 'f':
	  case 'g':
	  case 'E':
	  case 'G':
	    if (seen_double_big_d)
	      this_argclass = dec128float_arg;
	    else if (seen_big_d)
	      this_argclass = dec64float_arg;
	    else if (seen_big_h)
	      this_argclass = dec32float_arg;
	    else if (seen_big_l)
	      this_argclass = long_double_arg;
	    else
	      this_argclass = double_arg;

	    if (lcount || seen_h)
	      bad = 1;
	    break;

	  case '*':
	    error (_("`*' not supported for precision or width in printf"));

	  case 'n':
	    error (_("Format specifier `n' not supported in printf"));

	  case '\0':
	    error (_("Incomplete format specifier at end of format string"));

	  default:
	    error (_("Unrecognized format specifier '%c' in printf"), *f);
	  }

	if (bad)
	  error (_("Inappropriate modifiers to "
		   "format specifier '%c' in printf"),
		 *f);

	f++;

	sub_start = current_substring;

	if (lcount > 1 && USE_PRINTF_I64)
	  {
	    /* Windows' printf does support long long, but not the usual way.
	       Convert %lld to %I64d.  */
	    int length_before_ll = f - percent_loc - 1 - lcount;

	    strncpy (current_substring, percent_loc, length_before_ll);
	    strcpy (current_substring + length_before_ll, "I64");
	    current_substring[length_before_ll + 3] =
	      percent_loc[length_before_ll + lcount];
	    current_substring += length_before_ll + 4;
	  }
	else if (this_argclass == wide_string_arg
		 || this_argclass == wide_char_arg)
	  {
	    /* Convert %ls or %lc to %s.  */
	    int length_before_ls = f - percent_loc - 2;

	    strncpy (current_substring, percent_loc, length_before_ls);
	    strcpy (current_substring + length_before_ls, "s");
	    current_substring += length_before_ls + 2;
	  }
	else
	  {
	    strncpy (current_substring, percent_loc, f - percent_loc);
	    current_substring += f - percent_loc;
	  }

	*current_substring++ = '\0';

	prev_start = f;

	m_pieces.emplace_back (sub_start, this_argclass);
      }

  /* Record the remainder of the string.  */

  sub_start = current_substring;

  strncpy (current_substring, prev_start, f - prev_start);
  current_substring += f - prev_start;
  *current_substring++ = '\0';

  m_pieces.emplace_back (sub_start, literal_piece);
}

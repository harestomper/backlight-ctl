/*
 * fstools.c
 *
 *  Created on: 6 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "includes.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define PATH_SEPARATOR '/'
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
typedef struct string_t
{
  int len;
  int cap;
  char* s;
} string_t;
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
string_addn (string_t* s, char const* val, int len)
{
  if (len <= 0 || !val || !*val)
    return;
  else if (s->len + len + 1 >= s->cap)
    {
      s->s = realloc (s->s, (s->len + len) * 2);
      s->cap = (s->len + len) * 2;
      s->s[s->len] = 0;
    }

  if (len)
    {
      memcpy (s->s + s->len, val, len);
      s->len += len;
      s->s[s->len] = 0;
    }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t
fs_test (char const* path, fs_test_t test)
{
  struct stat st;
  int err = errno;
  bool_t retval = false;

  if (path && *path && stat (path, &st) == 0)
    {
      switch (test)
        {
        case FS_EXISTS:
          retval = true;
          break;
        case FS_IS_SOCK:
          retval = (S_ISSOCK (st.st_mode) == 0);
          break;
        case FS_IS_DIR:
          retval = (S_ISDIR (st.st_mode) == 0);
          break;
        case FS_IS_REG:
          retval = (S_ISREG (st.st_mode) == 0);
          break;
        }
    }

  errno = err;

  return retval;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline char const*
slash_strip (char const* p)
{
  while (p && *p && *p == PATH_SEPARATOR)
    p++;
  return p;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline char const*
slash_next (char const* p)
{
  while (p && *p && *p != PATH_SEPARATOR)
    p++;
  return p;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static char*
build_path_va (char const* first, va_list args)
{
  string_t str = { 0, 0, null };
  char const sep = PATH_SEPARATOR;
  char const* end;
  char const *element, *next;
  int len;
  int n_elems = 0;

  next = first;

  while (next)
    {
      element = next;
      next = va_arg (args, char*);

      len = str.len;
      end = element + strlen (element);

      if (*element == sep && n_elems)
        element = slash_strip (element);

      while (end > element + 1 && end[-1] == sep)
        end--;

      string_addn (&str, element, end - element);

      if (str.len > len && str.s[str.len - 1] != sep)
        string_addn (&str, &sep, 1);

      n_elems += (str.len > len);
    }

  while (str.len >= 1 && str.s[str.len - 1] == sep)
    str.len--;

  str.s[str.len] = 0;

  return str.s;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
char*
fs_path_join (char const* first, ...)
{
  char* retval;
  va_list args;

  va_start (args, first);
  retval = build_path_va (first, args);
  va_end (args);

  return retval;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t
fs_make_path (char const* path, int mode)
{
  struct stat st;
  char *dup, *saved_cwd, *elem;
  char const *start, *end;
  int rc = 0;

  saved_cwd = getcwd (null, 0);
  dup = strdup (path);
  start = dup;

  while (*start && rc == 0)
    {
      end = start + 1;

      // Skip all nonslash charactes
      for (end = start + 1; *end && end[-1] != '/'; end++)
        continue;

      // copy the current element, including the trailing slash
      elem = strndup (start, end - start);

      // Take an mode of the current dir
      stat (".", &st);

      // Try to go to the dir. If it's
      // unsuccessful, we try to create it
      // and again try to go into it.
      while (rc == 0 && chdir (elem) == -1)
        rc = mkdir (elem, st.st_mode);

      // reset the error if we tried to re-create the directory.
      if (errno == EEXIST)
        errno = 0;

      free (elem);

      // skip all duplicate slashes.
      start = slash_strip (end);
    }

  // If there were no errors, then we
  // are in the last created directory,
  // which has the same mode as its
  // parent. If another mode is specified,
  // then we want to replace it with it.
  if (rc == 0 && mode)
    rc = chmod (".", mode);

  rc = chdir (saved_cwd);

  free (saved_cwd);
  free (dup);

  return (rc == 0);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int
fs_getint (int fd)
{
  int val = 0;
  char c;

  lseek (fd, 0, SEEK_SET);

  while (read (fd, &c, sizeof (c)) == sizeof (c) && isdigit (c))
    val = val * 10 + (c - '0');

  return val;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
fs_setint (int fd, int val)
{
#define len 25
  char buf[len];
  char* p = buf + len;

  do
    {
      *--p = (val % 10) + '0';
      val /= 10;
    }
  while (val);

  lseek (fd, 0, SEEK_SET);

  if (write (fd, p, len - (p - buf)) < 0)
    eprintf ("%s", strerror (errno));
#undef len
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
char*
fs_stringf (char const* format, ...)
{
  va_list args;
  char* retval = null;
  int len;

  va_start (args, format);
  len = vsnprintf (null, 0, format, args);
  va_end (args);
  retval = malloc (len + 1);
  va_start (args, format);
  len = vsnprintf (retval, len + 1, format, args);
  va_end (args);

  return retval;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int
fs_open_socket (char const* path, sock_func_t func)
{
  int sock, rc;
  struct sockaddr_un addr;
  typedef int (*real_sock_func_t) (int, struct sockaddr const*, socklen_t);
  real_sock_func_t invoke = (real_sock_func_t) func;

  if (!path || !*path || !func)
    {
      errno = EADDRNOTAVAIL;
      return -1;
    }

  sock = socket (AF_UNIX, SOCK_STREAM, 0);

  switch (sock)
    {
    default:
      addr.sun_family = AF_UNIX;
      strcpy (addr.sun_path, path);

      if (invoke (sock, (struct sockaddr*) &addr, sizeof (addr)) == 0)
        break;
      /* no break */
    case -1:
      rc = errno;
      set_fd (sock, -1);
      errno = rc;
    }

  return sock;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

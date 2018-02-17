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
static void string_addn (string_t* s, char const* val, int len)
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

  return;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t fs_test (char const* path, fs_test_t test)
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
static inline char const* slash_strip (char const* p)
{
  while (p && *p && *p == PATH_SEPARATOR)
    p++;
  return p;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline char const* slash_next (char const* p)
{
  while (p && *p && *p != PATH_SEPARATOR)
    p++;
  return p;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static char* build_path_va (char const* first, va_list args)
{
  string_t str = { 0, 0, null };
  char const sep = PATH_SEPARATOR;

  while (first)
    {
      char const* p = first;
      char const* pend = p + strlen (first);
      char const* pit;

      while (p < pend)
        {
          switch (p[0])
            {
            case PATH_SEPARATOR:
              p++;
              break;

            case '.':
              if (p[1] == '.')
                {
                  while (str.len > 0 && str.s[str.len] != sep)
                    str.len--;
                  p += 2;
                  break;
                }
              else if (p[1] == sep)
                break;
              /* no break */

            default:
              pit = slash_next (p);
              string_addn (&str, &sep, 1);
              string_addn (&str, p, pit - p);
              p = pit;
            }
        }

      first = va_arg (args, char*);
    }

  return str.s;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
char* fs_path_join (char const* first, ...)
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
int _mkpath (char const* path, int mode)
{
  char const* path_it = path;
  char *current, *next;
  int result = 0;
  bool_t exists_before;

  if (*path_it == PATH_SEPARATOR)
    {
      path_it = slash_strip (path_it);
      path = path_it - 1;
    }
  else
    path_it = slash_next (path_it);

  if ((current = strndup (path, path_it - path)) == null)
    return ENOMEM;

  exists_before = fs_test (current, FS_EXISTS);

  if (exists_before || mkdir (current, mode) == 0)
    {
      errno = 0;
      path_it = slash_strip (path_it);

      if (*path_it && chdir (current) == 0)
        {
          if ((next = strdup (path_it)) != null)
            {
              result = _mkpath (next, mode);
              free (next);
            }
          else
            result = ENOMEM;
        }
      else
        result = errno;
    }
  else
    result = errno;

  if (result && !exists_before)
    remove (current);

  free (current);

  return result;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t fs_make_path (char const* path, int mode)
{
  char* cwd = getcwd (null, 0);
  int result = _mkpath (path, mode ? mode : (S_IRWXG | S_IRWXU | S_IRWXO));

  if (chdir (cwd) < 0)
    eprintf ("%s", strerror (errno));

  free (cwd);
  errno = result;

  return (result == 0);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int fs_getint (int fd)
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
void fs_setint (int fd, int val)
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
char* fs_stringf (char const* format, ...)
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
int fs_open_socket (char const* path, sock_func_t func)
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

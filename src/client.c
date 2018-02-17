/*
 * client.c
 *
 *  Created on: 4 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "includes.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline char** read_proc_commandline (int pid)
{
  char** retval = null;
  char* path;
  int retlen = 0;
  int fd;

  path = fs_stringf ("/proc/%i/cmdline", pid);

  if ((fd = open (path, O_RDONLY)) > -1)
    {
      char *str, *p, c;

      str = p = malloc (256);

      while (read (fd, &c, sizeof (c)) == sizeof (c))
        {
          *p++ = c;

          if (c == 0)
            {
              retval = realloc (retval, sizeof (str) * (retlen + 2));
              retval[retlen++] = strdup (str);
              retval[retlen] = null;
              p = str;
              *p = 0;
            }
        }

      set_fd (fd, -1);
      ckfree (str);
    }
  else
    eprintf ("%s", strerror (errno));

  ckfree (path);

  return retval;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t client_execute_stop_restart (client_t* self)
{
  int pid = 0, fd;
  bool_t result = false;

  if (fs_test (self->pidfile, FS_EXISTS))
    {
      if ((fd = open (self->pidfile, O_RDONLY)) == -1)
        eprintf ("%s", strerror (errno));
      else
        {
          pid = fs_getint (fd);
          set_fd (fd, -1);
        }
    }
  else if (fs_test (self->socketname, FS_IS_SOCK))
    {
      message_t msg;

      if ((fd = fs_open_socket (self->socketname, (sock_func_t) connect)) == -1)
        eprintf ("%s", strerror (errno));
      else if (write (fd, &self->msg, sizeof (self->msg)) < 0)
        eprintf ("%s", strerror (errno));
      else if (read (fd, &msg, sizeof (msg)) == sizeof (msg))
        pid = msg.type == TYPE_INT ? msg.v_int : 0;
      set_fd (fd, -1);
    }

  if (pid > 0)
    {
      char **cmdline, **it;

      cmdline = read_proc_commandline (pid);

      if (kill (pid, SIGINT) == -1)
        eprintf ("%s", strerror (errno));
      else if (self->msg.field == FIELD_RESTART && cmdline)
        {
          context_destroy ((context_t*) self);

          if (!(result = (execvp (cmdline[0], cmdline) == 0)))
            eprintf ("%s", strerror (errno));
          _exit (0);
        }
      else
        result = true;

      for (it = cmdline; it && *it; it++)
        ckfree (*it);
      ckfree (cmdline);
    }

  return result;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t client_execute (client_t* client)
{
  int sock, size;
  message_t result = MESSAGE_INIT;
  bool_t retval = false;

  context_spw_init ((context_t*) client);

  switch (client->msg.field)
    {
    case FIELD_STOP:
    case FIELD_RESTART:
      return client_execute_stop_restart (client);
    default:
      break;
    }

  size = sizeof (client->msg);
  sock = fs_open_socket (client->socketname, (sock_func_t) connect);

  if (sock == -1 || write (sock, &client->msg, size) == -1)
    eprintf ("%s", strerror (errno));
  else
    {
      do
        {
          switch (read (sock, &result, sizeof (result)))
            {
            case -1:
              eprintf ("%s", strerror (errno));
              break;

            default:
              retval = true;

              switch (result.type)
                {
                case TYPE_INT:
                  printf ("%i\n", result.v_int);
                  break;

                case TYPE_STRING:
                  printf ("%s\n", result.v_str);
                  break;

                case TYPE_NONE:
                  break;
                case TYPE_ERROR:
                  eprintf ("%s", result.v_str);
                  retval = false;
                }
            }
        }
      while (retval && result.read_more);
    }

  close (sock);

  if (retval)
    printf ("Done\n");

  return retval;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t set_message (client_t* ctx, message_t const* msg)
{
  if (ctx->msg.field != FIELD_NONE)
    return false;

  ctx->msg = *msg;

  return true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void client_clear (client_t* self)
{
  if (!self)
    return;

  ckfree (self->pidfile);
  ckfree (self->socketname);
  ckfree (self->workdir);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t set_pidfile (client_t* self, message_t const* msg)
{
  return SETSTR (self->pidfile, msg->v_str);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t set_sockname (client_t* self, message_t const* msg)
{
  return SETSTR (self->socketname, msg->v_str);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t set_workdir (client_t* self, message_t const* msg)
{
  return SETSTR (self->workdir, msg->v_str);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void client_init (context_t* ctx)
{
  if (!ctx)
    return;

  memset (ctx, 0, sizeof (client_t));

  context_bind (ctx, INC, set_message);
  context_bind (ctx, DEC, set_message);
  context_bind (ctx, ON, set_message);
  context_bind (ctx, OFF, set_message);
  context_bind (ctx, SWITCH, set_message);
  context_bind (ctx, STOP, set_message);
  context_bind (ctx, RESTART, set_message);
  context_bind (ctx, SAVED, set_message);
  context_bind (ctx, LIST, set_message);
  context_bind (ctx, MINIMAL, set_message);
  context_bind (ctx, NUM_LEVELS, set_message);
  context_bind (ctx, TRANSITION, set_message);
  context_bind (ctx, DEVNAME, set_message);
  context_bind (ctx, PIDFILE, set_pidfile);
  context_bind (ctx, SOCKNAME, set_sockname);
  context_bind (ctx, WORKDIR, set_workdir);

  ctx->run = (exec_func_t) client_execute;
  ctx->clear = (destroy_func_t) client_clear;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

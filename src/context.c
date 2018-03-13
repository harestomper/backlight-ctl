/*
 * context.c
 *
 *  Created on: 2 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "includes.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
context_spw_init (context_t* self)
{
  if (!self->workdir)
    self->workdir = strdup (statics_defaults[DEFAULT_WORKDIR].v_str);

  if (!self->socketname)
    self->socketname = fs_path_join (self->workdir,
                                     statics_defaults[DEFAULT_SOCKET].v_str,
                                     null);

  if (!self->pidfile)
    self->pidfile = fs_path_join (self->workdir,
                                  statics_defaults[DEFAULT_PIDFILE].v_str,
                                  null);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static option_t const*
find_option (option_t const* options, char const* name)
{
  if (!name || !*name || !options)
    return null;

  int so = 0;
  char const* lo = null;
  int name_len = strlen (name);

  if (name_len == 2 && *name == '-')
    so = name[1];
  else if (name_len > 2 && name[1] == '-')
    lo = name + 2;
  else if (name_len > 2 && *name == '-')
    return null;
  else
    lo = name;

  while (options->field != FIELD_NONE)
    {
      if (options->short_name && so && options->short_name == so)
        return options;
      else if (options->long_name && lo && !strcmp (options->long_name, lo))
        return options;

      options++;
    }

  return null;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static context_t*
context_allocate (void)
{
  context_t* ret;

  ret = (context_t*) malloc (sizeof (*ret));

  if (!ret)
    eprintf ("%s", "No memory");
  else
    memset (ret, 0, sizeof (*ret));

  return ret;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
dummy_server_start (void)
{
  return true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
context_t*
context_create (int argc, char** argv)
{
  option_t const* opt = null;
  context_t* ctx = null;

  switch (argc)
    {
    case 1:
      return null;

    default:
      opt = find_option (statics_options, argv[1]);
    }

  if (opt == null)
    {
      eprintf ("Bad command '%s'. Seek usage.", argv[1]);
      usage_print (argc, argv);
      return null;
    }

  switch (opt->field)
    {
    default:
      ctx = context_allocate ();
      client_init (ctx);
      break;

    case FIELD_START:
      ctx = context_allocate ();
      server_init (ctx);
      context_bind (ctx, START, dummy_server_start);
      break;

    case FIELD_NONE:
      eprintf ("%s", "Missong command");
      return null;
    }

  return ctx;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
parse_string (char** args, char** end, message_t* msg)
{
  int len;
  int smax = sizeof (msg->v_str);

  if (args + 1 >= end)
    return 0;

  len = strlen (args[1]);
  len = len < smax - 1 ? len : smax - 1;
  memcpy (msg->v_str, args[1], len);
  msg->v_str[len] = 0;

  return 2;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
parse_int (char** args, char** end, message_t* msg)
{
  if (args + 1 >= end)
    return 0;

  int val = 0;
  char const* p;

  for (p = args[1]; isdigit (*p); p++)
    val = (val * 10) + (*p - '0');

  if (p > args[1])
    {
      msg->v_int = val;
      return 2;
    }

  return 0;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
parse_error (char** args __attribute__ ((unused)),
             char** end __attribute__ ((unused)),
             message_t* msg __attribute__ ((unused)))
{
  return 0;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
parse_none (char** args, char** end, message_t* msg __attribute__ ((unused)))
{
  return (args >= end) ? 0 : 1;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t
context_configure (context_t* ctx, int argc, char** argv)
{
  typedef int (*parse_t) (char**, char**, message_t*);
  option_t const* opt;
  parse_t parse[] = { parse_none, parse_int, parse_string, parse_error };
  char **args, **args_end = argv + argc;
  message_t msg = MESSAGE_INIT;
  int rc;

  if (!ctx)
    {
      eprintf ("%s", "Context is NULL");
      return false;
    }

  args = argv + 1;
  rc = 1;

  while (args < args_end && rc > 0)
    {
      opt = find_option (statics_options, *args);

      if (opt)
        {
          memset (&msg, 0, sizeof (msg));
          msg.field = opt->field;
          msg.type = statics_types[opt->field];

          rc = parse[msg.type](args, args_end, &msg);

          if (rc == 0)
            eprintf ("Missing argument for '%s'", *args);
          else if (rc < 0 || !context_perform (ctx, &msg))
            {
              eprintf ("Bad command '%s'", *args);
              rc = -1;
            }
          else
            args += rc;
        }
      else
        {
          eprintf ("Bad command line argument '%s'", *args);
          rc = 0;
        }
    }

  return (bool_t) (rc > 0 && args >= args_end);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t
context_perform (context_t* ctx, message_t const* msg)
{
  if (ctx->fields[msg->field])
    return (*ctx->fields[msg->field]) (ctx, msg);

  return false;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t
context_run (context_t* ctx)
{
  bool_t retval;

  if (!ctx->run)
    return false;

  retval = (*ctx->run) (ctx);

  return retval;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
context_destroy (context_t* ctx)
{
  if (!ctx)
    return;

  if (ctx->clear)
    (*ctx->clear) (ctx);

  free (ctx);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

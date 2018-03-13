/*
 * server.c
 *
 *  Created on: 4 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "includes.h"

#include <asm-generic/socket.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define MAX_POLL_SIZE 10
#define POLL_TIMEOUT 20
#define POLL_TIMEOUT_WAIT -1
#define BACKLIGHT "/", "sys", "class", "backlight"
#define fround(x) __extension__(((__typeof__(x)) ((int) ((x) + 0.5))))
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static volatile bool_t g_total_quit = false;
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
typedef struct config_t
{
  int minimal;
  int num_levels;
  int transition;
  int saved_level;
  char devname[STRSIZE];
} config_t;
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
typedef struct server_message_t
{
  message_t msg;
  int socket;
} server_message_t;
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t server_execute (server_t* ctx);
void server_clear (server_t* self);
static bool_t server_set_devname (server_t* self, char const* devname);
static bool_t server_save (server_t* self, field_t field);
static bool_t server_load (server_t* self, field_t field);
static int server_is_running (server_t* self);
static bool_t server_prepare (server_t* self);
static void server_adjust (server_t* self);
static bool_t server_start (server_t* self);
static bool_t server_config (server_t* self, message_t const* msg);
static bool_t server_command (server_t* self, message_t const* msg);
static bool_t cb_server_stop (server_t* self, server_message_t const* msg);
static bool_t cb_server_get_saved (server_t* self, server_message_t const* msg);
static bool_t cb_server_device_list (server_t* self,
                                     server_message_t const* msg);
static bool_t device_name_is_valid (char const* name);
static int get_device_max (char const* devname);
static char* find_device (char* dest, int dest_size);
static bool_t server_device_set (server_t* self, int value);
static int server_device_get (server_t* self);
static void set_signals (void);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline int
reply (int sock, void* data, int size)
{
  return send (sock, data, size, MSG_NOSIGNAL);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
server_init (context_t* ctx)
{
  server_t* server = (server_t*) ctx;

  if (!ctx)
    return;

  server->timeout = POLL_TIMEOUT;
  server->socket = -1;

  context_bind (ctx, INC, server_command);
  context_bind (ctx, DEC, server_command);
  context_bind (ctx, ON, server_command);
  context_bind (ctx, OFF, server_command);
  context_bind (ctx, SWITCH, server_command);
  context_bind (ctx, STOP, cb_server_stop);
  context_bind (ctx, RESTART, cb_server_stop);
  context_bind (ctx, SAVED, cb_server_get_saved);
  context_bind (ctx, LIST, cb_server_device_list);
  context_bind (ctx, MINIMAL, server_config);
  context_bind (ctx, NUM_LEVELS, server_config);
  context_bind (ctx, TRANSITION, server_config);
  context_bind (ctx, DEVNAME, server_config);
  context_bind (ctx, SOCKNAME, server_config);
  context_bind (ctx, WORKDIR, server_config);
  context_bind (ctx, PIDFILE, server_config);
  context_bind (ctx, DAEMON, server_config);
  context_bind (ctx, CONFIG, server_config);

  ctx->run = (exec_func_t) server_execute;
  ctx->clear = (destroy_func_t) server_clear;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool_t
server_execute (server_t* self)
{
  bool_t result;
  int fd;

  self->minimal = statics_defaults[DEFAULT_MINIMAL].v_int;
  self->num_levels = statics_defaults[DEFAULT_NUM_LEVELS].v_int;
  self->transition = statics_defaults[DEFAULT_TRANSITION].v_int;

  if (self->daemon && daemon (false, true) < 0)
    eprintf ("%s", strerror (errno));

  if (!server_prepare (self))
    return false;
  else
    {
      switch (fd = creat (self->pidfile, 00664))
        {
        case -1:
          eprintf ("%s: %s", self->pidfile, strerror (errno));
          break;

        default:
          fs_setint (fd, getpid ());
          close (fd);
        }
    }

  set_signals ();

  result = server_start (self);

  unlink (self->pidfile);
  unlink (self->socketname);

  return result;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
server_clear (server_t* self)
{
  if (!self)
    return;

  ckfree (self->pidfile);
  ckfree (self->socketname);
  ckfree (self->workdir);
  ckfree (self->config);
  ckfree (self->dev.name);
  set_fd (self->dev.set, -1);
  set_fd (self->dev.get, -1);
  set_fd (self->socket, -1);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_set_devname (server_t* self, char const* devname)
{
  if (device_name_is_valid (devname))
    {
      float tmp;
      char *get, *set;

      ckfree (self->dev.name);
      set_fd (self->dev.set, -1);
      set_fd (self->dev.get, -1);

      set = fs_path_join (BACKLIGHT, devname, "brightness", null);
      get = fs_path_join (BACKLIGHT, devname, "actual_brightness", null);

      self->dev.name = strdup (devname);
      self->dev.max = get_device_max (devname);
      self->dev.set = open (set, O_WRONLY);
      self->dev.get = open (get, O_RDONLY);

      self->num_levels = MIN (self->dev.max, self->num_levels);
      self->minimal = self->minimal >= self->dev.max ? 0 : self->minimal;
      tmp = (self->dev.max - self->minimal) / (float) self->num_levels;
      self->level_size = fround (tmp);
      self->level = self->num_levels >> 1;

      ckfree (set);
      ckfree (get);

      return true;
    }

  return false;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_save (server_t* self, field_t field)
{
  int n_fields_to_save = 0;
  config_t conf;
  int fd;

  switch (fd = open (self->config, O_RDONLY))
    {
    default:
      if (read (fd, &conf, sizeof (conf)) == sizeof (conf))
        break;
      /* no break */
    case -1:
      memset (conf.devname, 0, sizeof (conf.devname));
      conf.minimal = -1;
      conf.num_levels = -1;
      conf.saved_level = -1;
      conf.transition = -1;
    }

  set_fd (fd, -1);

  switch (field)
    {
    default:
      return false;

    case FIELD_TRANSITION:
      if (self->transition >= 0 && conf.transition != self->transition)
        {
          conf.transition = self->transition;
          n_fields_to_save++;
        }

      break;

    case FIELD_MINIMAL:
      if (self->minimal >= 0 && conf.minimal != self->minimal)
        {
          conf.minimal = self->minimal;
          n_fields_to_save++;
        }
      break;

    case FIELD_NUM_LEVELS:
      if (self->num_levels >= 0 && conf.num_levels != self->num_levels)
        {
          conf.num_levels = self->num_levels;
          n_fields_to_save++;
        }
      break;

    case FIELD_DEVNAME:
      if (self->dev.name && strcmp (self->dev.name, conf.devname))
        {
          memset (conf.devname, 0, sizeof (conf.devname));
          strcpy (conf.devname, self->dev.name);
          n_fields_to_save++;
        }
      break;

    case FIELD_SAVED:
      if (self->level >= 0 && conf.saved_level != self->level)
        {
          conf.saved_level = self->level;
          n_fields_to_save++;
        }
    }

  if (n_fields_to_save)
    {
      switch (fd = creat (self->config, 00664))
        {
        default:
          if (write (fd, &conf, sizeof (conf)) == sizeof (conf))
            {
              set_fd (fd, -1);
              return true;
            }
          set_fd (fd, -1);
          /* no break */
        case -1:
          return false;
        }
    }

  return true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_load (server_t* self, field_t field)
{
  config_t conf;
  int size = sizeof (conf);
  int fd;

  switch (fd = open (self->config, O_RDONLY))
    {
    default:
      if (read (fd, &conf, size) == size)
        break;
      /* no break */
    case -1:
      memset (&conf, -1, size);
      memset (conf.devname, 0, sizeof (conf.devname));
    }

  set_fd (fd, -1);

  if (conf.minimal < 0)
    conf.minimal = statics_defaults[DEFAULT_MINIMAL].v_int;

  if (conf.num_levels < 0)
    conf.num_levels = statics_defaults[DEFAULT_NUM_LEVELS].v_int;

  if (conf.transition < 0)
    conf.transition = statics_defaults[DEFAULT_TRANSITION].v_int;

  switch (field)
    {
    default:
      return false;

    case FIELD_NONE:

    case FIELD_DEVNAME:
      if (!conf.devname[0]
          && find_device (conf.devname, sizeof (conf.devname)) == null)
        return false;

      if (!server_set_devname (self, conf.devname))
        return false;
      else if (field != FIELD_NONE)
        break;
      /* no break */

    case FIELD_MINIMAL:
      self->minimal = conf.minimal >= self->dev.max ? 0 : conf.minimal;

      if (field != FIELD_NONE)
        break;
      /* no break */

    case FIELD_NUM_LEVELS:
      {
        self->num_levels = MIN (self->dev.max, conf.num_levels);
        float tmp = (self->dev.max - self->minimal) / (float) self->num_levels;
        self->level_size = fround (tmp);

        if (field != FIELD_NONE)
          break;
      }
      /* no break */
    case FIELD_SAVED:
      if (conf.saved_level >= 0 && conf.saved_level < self->num_levels)
        self->level = conf.saved_level;
      else
        self->level = self->num_levels >> 1;

      if (field != FIELD_NONE)
        break;
      /* no break */

    case FIELD_TRANSITION:
      self->transition = conf.transition;
    }

  return true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
server_is_running (server_t* self)
{
  char spid[16];
  int pid, fd;

  if ((fd = open (self->pidfile, O_RDONLY)) < 0)
    return 0;

  close (fd);

  if ((pid = fs_getint (fd)) == 0)
    return 0;

  snprintf (spid, 16, "/proc/%i", pid);

  if (fs_test (spid, FS_EXISTS))
    return pid;

  return 0;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_prepare (server_t* self)
{
  int pid;

  context_spw_init ((context_t*) self);

  if (!self->config)
    self->config = fs_path_join (self->workdir,
                                 statics_defaults[DEFAULT_CONFIG].v_str, null);

  if ((pid = server_is_running (self)) > 0)
    {
      eprintf ("The server is already running and has an PID: %d\n", pid);
      return false;
    }

  if (!fs_make_path (self->workdir, 0))
    {
      eprintf ("%s", strerror (errno));
      return false;
    }

  if (!server_load (self, FIELD_NONE))
    return false;

  unlink (self->socketname);

  switch (self->socket = fs_open_socket (self->socketname, (sock_func_t) bind))
    {
    default:
      if (chmod (self->socketname, 00666) == 0)
        return true;
      /* no break */
    case -1:
      eprintf ("%s", strerror (errno));
      set_fd (self->socket, -1);
      unlink (self->socketname);
    }

  return false;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
server_adjust (server_t* self)
{
  int target, newv, current, diff, step;

  if (self->level >= 0)
    target = self->level_size * (float) self->level + self->minimal;
  else
    target = 0;

  target = MIN (target, self->dev.max);

  if (self->transition <= POLL_TIMEOUT)
    {
      if (server_device_set (self, target))
        self->timeout = POLL_TIMEOUT;
      else
        self->timeout = POLL_TIMEOUT_WAIT;
    }
  else if ((current = server_device_get (self)) != target)
    {
      diff = target - current;
      step = fround (self->level_size / (float) MAX (self->transition, 1.0)
                     * diff);
      newv = (diff < 0) ? MAX (current + MIN (step, -1), target)
                        : MIN (current + MAX (step, 1), target);

      if (server_device_set (self, newv))
        self->timeout = POLL_TIMEOUT;
      else
        self->timeout = POLL_TIMEOUT_WAIT;
    }
  else
    {
      if (self->level >= 0)
        server_save (self, FIELD_SAVED);
      self->timeout = POLL_TIMEOUT_WAIT;
    }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline void
accept_connection (int sock, struct pollfd* start, struct pollfd* end)
{
  for (; start < end && start->fd >= 0; start++)
    ;

  if (start < end)
    start->fd = accept (sock, null, null);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline void
handle_message (server_t* self, struct pollfd* ps)
{
  server_message_t smsg = { MESSAGE_INIT, -1 };
  message_t* msg = (message_t*) &smsg;
  int size = sizeof (smsg.msg);
  int rc;

  if ((rc = recv (ps->fd, msg, size, 0)) < 0)
    {
      seterrf (msg->v_str, "Failed to receive message:%s", strerror (errno));
      msg->type = TYPE_ERROR;
    }
  else if (rc != size)
    {
      seterrf (msg->v_str, "%s", "Received a broken message");
      msg->type = TYPE_ERROR;
    }
  else
    {
      smsg.socket = ps->fd;
      context_perform ((context_t*) self, msg);
    }

  reply (ps->fd, msg, size);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_start (server_t* self)
{
  struct pollfd ps[MAX_POLL_SIZE];
  struct pollfd* psit;
  struct pollfd* psend = ps + MAX_POLL_SIZE;
  int ready;
  int on = 1;
  bool_t result;

  result = (setsockopt (self->socket, SOL_SOCKET, SO_REUSEADDR, (char*) &on,
                        sizeof (on))
            == 0);
  result = result && (fcntl (self->socket, F_SETFL, O_NONBLOCK) == 0);
  result = result && (listen (self->socket, MAX_POLL_SIZE) == 0);

  for (psit = ps; psit < psend && result; psit++)
    {
      psit->fd = -1;
      psit->events = POLLIN;
      psit->revents = 0;
    }

  ps->fd = self->socket;

  while (result && !g_total_quit)
    {
      server_adjust (self);

      switch ((ready = poll (ps, psend - ps, self->timeout)))
        {
        case 0:
          break;

        case -1:
          if (!g_total_quit)
            {
              eprintf ("%s", strerror (errno));
              result = false;
            }
          break;

        default:
          {
            for (psit = ps; psit < psend; psit++)
              {
                if (g_total_quit || ready <= 0)
                  break;
                else if (psit->revents && psit->fd == self->socket)
                  accept_connection (self->socket, ps, psend);
                else if (psit->revents & POLLHUP)
                  set_fd (psit->fd, -1);
                else if (psit->revents & (POLLIN | POLLPRI))
                  handle_message (self, psit);

                ready -= (psit->revents != 0);
              }
          }
        }
    }

  if (!result && !g_total_quit)
    eprintf ("%s", strerror (errno));

  for (psit = ps; psit < psend; psit++)
    if (psit->fd >= 0)
      close (psit->fd);

  return (result || g_total_quit);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_config (server_t* self, message_t const* msg)
{
  bool_t result = false;

  switch (msg->field)
    {
    case FIELD_CONFIG:
      return SETSTR (self->config, msg->v_str);

    case FIELD_WORKDIR:
      return SETSTR (self->workdir, msg->v_str);

    case FIELD_PIDFILE:
      return SETSTR (self->pidfile, msg->v_str);

    case FIELD_SOCKNAME:
      return SETSTR (self->socketname, msg->v_str);

    case FIELD_DAEMON:
      self->daemon = true;
      return true;

    case FIELD_DEVNAME:
      if (server_set_devname (self, msg->v_str))
        return server_save (self, msg->field);
      else
        return false;
      break;

    default:
      if (msg->type == TYPE_INT && msg->v_int >= 0)
        {
          switch (msg->field)
            {
            case FIELD_MINIMAL:
              self->minimal = msg->v_int;
              break;
            case FIELD_TRANSITION:
              self->transition = msg->v_int;
              break;
            case FIELD_NUM_LEVELS:
              self->num_levels = msg->v_int;
              break;
            default:
              break;
            }
          result = true;
        }
    }

  if (result)
    return server_save (self, msg->field);

  return false;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_command (server_t* self, message_t const* msg)
{
  switch (msg->field)
    {
    case FIELD_INC:
      if (self->level < 0)
        server_load (self, FIELD_SAVED);
      else
        self->level += (self->level + 1 <= self->num_levels);
      break;

    case FIELD_DEC:
      if (self->level < 0)
        server_load (self, FIELD_SAVED);
      else
        self->level -= (self->level - 1 >= 0);
      break;

    case FIELD_ON:
      server_load (self, FIELD_SAVED);
      break;

    case FIELD_OFF:
      server_save (self, FIELD_SAVED);
      self->level = -1;
      break;

    case FIELD_SWITCH:
      if (self->level < 0)
        server_load (self, FIELD_SAVED);
      else
        {
          server_save (self, FIELD_SAVED);
          self->level = -1;
        }
      break;

    default:
      return false;
    }

  return true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
cb_server_stop (server_t* self __attribute__ ((unused)),
                server_message_t const* smsg)
{
  message_t rep;

  rep.field = FIELD_NONE;
  rep.type = TYPE_INT;
  rep.v_int = getpid ();
  rep.read_more = false;

  return (reply (smsg->socket, &rep, sizeof (rep)) == sizeof (rep));
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
cb_server_get_saved (server_t* self, server_message_t const* msg)
{
  message_t res = MESSAGE_INIT;

  res.field = msg->msg.field;
  res.type = TYPE_INT;
  res.v_int = self->level;

  return (reply (msg->socket, &res, sizeof (res)) == sizeof (res));
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
cb_server_device_list (server_t* self __attribute__ ((unused)),
                       server_message_t const* msg)
{
  DIR* dir;
  char* path;
  bool_t result = false;
  message_t res;
  int size = sizeof (res);

  path = fs_path_join (BACKLIGHT, null);
  dir = opendir (path);
  ckfree (path);

  if (dir != null)
    {
      struct dirent* ent;

      res.field = msg->msg.field;
      res.type = TYPE_STRING;
      res.read_more = true;
      result = true;

      while ((ent = readdir (dir)) != null && result)
        {
          if (strcmp (ent->d_name, "..") == 0 || strcmp (ent->d_name, ".") == 0)
            continue;

          memset (res.v_str, 0, sizeof (res.v_str));
          strcpy (res.v_str, ent->d_name);
          result = (reply (msg->socket, &res, size) == size);
        }

      if (result)
        {
          res.type = TYPE_NONE;
          res.read_more = false;
          *res.v_str = 0;
          result = (reply (msg->socket, &res, size) == size);
        }

      closedir (dir);
    }
  else
    {
      res.type = TYPE_ERROR;
      seterrf (res.v_str, "%s", strerror (errno));
      reply (msg->socket, &res, size);
    }

  return result;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
device_name_is_valid (char const* name)
{
  char* setter;
  char* getter;
  bool_t result;

  if (!name || !*name)
    return false;

  setter = fs_path_join (BACKLIGHT, name, "brightness", null);
  getter = fs_path_join (BACKLIGHT, name, "actual_brightness", null);
  result = (access (setter, W_OK) == 0 && access (getter, R_OK) == 0);
  ckfree (setter);
  ckfree (getter);

  return result;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
get_device_max (char const* devname)
{
  char* path;
  int value = 0;
  int fd;

  path = fs_path_join (BACKLIGHT, devname, "max_brightness", null);

  if ((fd = open (path, O_RDONLY)) >= 0)
    {
      value = fs_getint (fd);
      close (fd);
    }

  ckfree (path);

  return value;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static char*
find_device (char* dest, int dest_size)
{
  char* path;
  DIR* dir;
  int len;

  dest[0] = 0;
  path = fs_path_join (BACKLIGHT, null);
  dir = opendir (path);

  ckfree (path);

  if (dir != null)
    {
      struct dirent* ent;
      char bestdev[STRSIZE];
      int value;
      int max = 0;

      *bestdev = 0;

      while ((ent = readdir (dir)) != null)
        {
          if (ent->d_name[0] == '.' || strncmp (ent->d_name, "..", 2) == 0)
            continue;

          value = get_device_max (ent->d_name);

          if (value > max)
            {
              strcpy (bestdev, ent->d_name);
              max = value;
            }
        }

      closedir (dir);

      if (*bestdev)
        {
          if (dest)
            {
              len = strlen (bestdev);

              if (len >= dest_size)
                return null;

              memcpy (dest, bestdev, len);
              memset (dest + len, 0, dest_size - len);
              return dest;
            }
          else
            return strdup (bestdev);
        }
    }

  return null;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool_t
server_device_set (server_t* self, int value)
{
  fs_setint (self->dev.set, value);

  return (server_device_get (self) == value);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int
server_device_get (server_t* self)
{
  return fs_getint (self->dev.get);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
signal_handler (int signum)
{
  char const* signame = "";

#define sig(x)                                                                 \
  case x:                                                                      \
    signame = #x;                                                              \
    break

  switch (signum)
    {
      sig (SIGINT);
      sig (SIGHUP);
      sig (SIGTERM);
    }
#undef sig

  printf ("\r\r\r\rTerminating the listener by signal %s\n", signame);

  g_total_quit = true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
set_signals (void)
{
  if (signal (SIGINT, signal_handler) == SIG_IGN)
    signal (SIGINT, SIG_IGN);
  if (signal (SIGHUP, signal_handler) == SIG_IGN)
    signal (SIGHUP, SIG_IGN);
  if (signal (SIGTERM, signal_handler) == SIG_IGN)
    signal (SIGTERM, SIG_IGN);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

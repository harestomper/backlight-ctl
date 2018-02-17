/*
 * context.h
 *
 *  Created on: 1 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

#ifndef SRC_CONTEXT_H_
#define SRC_CONTEXT_H_
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
struct context_t
{
  union
  {
    struct
    {
      char* socketname;
      char* pidfile;
      char* workdir;
    };
    union
    {

      struct client_t
      {
        char* socketname;
        char* pidfile;
        char* workdir;
        message_t msg;
      } client;

      struct server_t
      {
        char* socketname;
        char* pidfile;
        char* workdir;
        char* config;
        bool_t daemon;
        int socket;
        int timeout;
        int transition;
        int minimal;
        int num_levels;
        int level_size;
        int level;

        struct
        {
          int max;
          int get;
          int set;
          char* name;
        } dev;
      } server;
    } data;
  };

  destroy_func_t clear;
  exec_func_t run;
  callback_t fields[FIELD_NUM];
};
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define context_bind(c, m, f)                                                  \
  (((context_t*) (c))->fields[FIELD_##m] = ((callback_t) (f)))
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
context_t* context_create (int argc, char** argv);
bool_t context_configure (context_t* ctx, int argc, char** argv);
bool_t context_perform (context_t* ctx, message_t const* msg);
bool_t context_run (context_t* ctx);
void context_destroy (context_t* ctx);
void context_spw_init (context_t* self);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif /* SRC_CONTEXT_H_ */

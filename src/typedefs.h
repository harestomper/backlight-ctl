/*
 * typedefs.h
 *
 *  Created on: 11 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

#ifndef SRC_TYPEDEFS_H_
#define SRC_TYPEDEFS_H_

typedef struct option_t option_t;
typedef struct message_t message_t;
typedef union default_value_t default_value_t;
typedef struct context_t context_t;
typedef struct client_t client_t;
typedef struct server_t server_t;

typedef bool_t (*callback_t) (context_t*, message_t const*);
typedef void (*destroy_func_t) (context_t*);
typedef bool_t (*exec_func_t) (context_t*);

#endif /* SRC_TYPEDEFS_H_ */

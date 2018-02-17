/*
 * main.c
 *
 *  Created on: 4 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

#include "includes.h"

int main (int argc, char** argv)
{
    context_t* ctx = null;
    int code = 0;

    if (argc == 1)
        usage_print (argc, argv);
    else if (!(ctx = context_create (argc, argv)))
        code = 1;
    else if (!context_configure (ctx, argc, argv))
        code = 2;
    else if (!context_run (ctx))
        code = 3;

    context_destroy (ctx);

    return code;
}

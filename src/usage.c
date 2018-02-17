/*
 * usage.c
 *
 *  Created on: 2 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "includes.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define WIDTH 80
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define strvfree(ss) do { \
    char **__p; \
    for (__p = ss; __p && *__p; __p++) { if (*__p) free (*__p); } \
    if (ss) free (ss); \
} while (0)
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static char* add_indent (char* p, size_t count)
{
    return ((char*) memset (p, ' ', count)) + count;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static char* add_string (char* p, char const* str)
{
    size_t len = strlen (str);
    return ((char*) memcpy (p, str, len)) + len;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static char** usage_reformat_description (char const* desc, int width)
{
    int mid = width / 2;
    size_t slen = strlen (desc);
    char buf[slen + 1];
    char* end = buf + slen;
    char* p = buf;
    char* start;
    char* prev_space = null;
    char** result = null;
    int result_len = 0;

    strcpy (buf, desc);

    for (start = p; start <= end; p++)
    {
        if (*p == '\n' || *p == '\r')
            *p = 0;
        else if (isspace (*p))
            prev_space = p;
        else if (p >= start + width)
        {
            if (isspace (*p) || (prev_space && prev_space - start >= mid))
            {
                if (!isspace (*p))
                    p = prev_space;
                *p = 0;
            }
        }

        if (!*p)
        {
            result = realloc (result, sizeof (*result) * (result_len + 2));
            result[result_len] = strdup (start);
            result[result_len + 1] = null;
            result_len++;
            start = p = p + 1;
            prev_space = null;
        }
    }

    return result;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void usage_print (int argc __attribute__ ((unused)),
                  char** argv)
{
    char const* argument = " <argument>";
    option_t const* it;
    int desc_indent = 0;
    int opt_indent = 2;
    int len;
    char sops[BUFFER_SIZE];
    char* p;
    char** desc, **descp;

    for (it = statics_options; it->field != FIELD_NONE; it++)
    {
        if (statics_types[it->field] != TYPE_NONE || !it->description)
            continue;

        len = (it->short_name) ? 2 : 0;
        len += (it->long_name) ? strlen (it->long_name) : 0;
        len += (it->short_name && it->long_name) ? 2 : 0;
        desc_indent = desc_indent < len ? len : desc_indent;
    }

    desc_indent += opt_indent * 2;

    printf ("Usage:\n");
    printf ("  %s [<option>] [<argument>] ...\n", argv[0]);
    printf ("\nOptions:\n");

    for (it = statics_options; it->field != FIELD_NONE; it++)
    {
        if (!it->description || !*it->description)
            continue;

        p = add_indent (sops, opt_indent);

        if (it->short_name)
        {
            *p++ = '-';
            *p++ = it->short_name;
        }

        if (it->long_name)
        {
            if (it->short_name)
                p = add_string (p, ", --");

            p = add_string (p, it->long_name);
        }

        if (statics_types[it->field] != TYPE_NONE)
            p = add_string (p, argument);

        if (desc_indent <= p - sops)
        {
            *p++ = '\n';
            p = add_indent (p, desc_indent);
        }
        else
            p = add_indent (p, desc_indent - (p - sops));

        descp = desc = usage_reformat_description (it->description,
        WIDTH - desc_indent);

        do
        {
            p = add_string (p, *descp);
            descp++;

            if (*descp)
            {
                *p++ = '\n';
                p = add_indent (p, desc_indent);
            }
        } while (*descp);

        strvfree (desc);

        if (it->defix != DEFAULT_NONE)
        {
            switch (statics_types[it->field])
            {
                default:
                    break;

                case TYPE_INT:
                    *p++ = '\n';
                    p = add_indent (p, desc_indent);
                    p += snprintf (p, STRSIZE, "(Default: %i)\n",
                    statics_defaults[it->defix].v_int);
                    break;

                case TYPE_STRING:
                    *p++ = '\n';
                    p = add_indent (p, desc_indent);
                    p += snprintf (p, STRSIZE, "(Default: %s)\n",
                    statics_defaults[it->defix].v_str);
            }
        }

        *p = 0;
        printf ("%s\n", sops);
    }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


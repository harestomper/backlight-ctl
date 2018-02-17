/*
 * statics.c
 *
 *  Created on: 9 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

#include "includes.h"

option_t const* __options (void)
{
    static option_t ops[] = {
            { FIELD_WORKDIR,    'w', "workdir",        "Use the specified directory "
                                                                "for config and saves.",                     DEFAULT_WORKDIR },

            { FIELD_SOCKNAME,   's', "socket",         "Use the specified socket file.",                    DEFAULT_SOCKET },

            { FIELD_PIDFILE,    'p', "pidfile",        "Use the specified PID file.",                       DEFAULT_PIDFILE },

            { FIELD_CONFIG,     'c', "config",         "Use the specified config file.",                    DEFAULT_CONFIG },

            { FIELD_DAEMON,     'd', "daemon",         "Start the server in background",                    DEFAULT_NONE },

            { FIELD_INC,        0,   "increase",       "Increase brightness",                               DEFAULT_NONE },
            { FIELD_INC,        0,   "up",             "Alias for 'increase'",                              DEFAULT_NONE },
            { FIELD_DEC,        0,   "decrease",       "Decrease brightness",                               DEFAULT_NONE },
            { FIELD_DEC,        0,   "dn",             "Alias for 'decrease'",                              DEFAULT_NONE },
            { FIELD_ON,         0,   "on",             "Turn on the disabled display",                      DEFAULT_NONE },
            { FIELD_OFF,        0,   "off",            "Turn off the enabled display",                      DEFAULT_NONE },

            { FIELD_SWITCH,     0,   "switch",         "Change the state of the dispalay",                  DEFAULT_NONE },

            { FIELD_STOP,       0,   "stop",           "Stop the server",                                   DEFAULT_NONE },
            { FIELD_START,      0,   "start",          "Start the server",                                  DEFAULT_NONE },
            { FIELD_RESTART,    0,   "restart",        "Restart server",                                    DEFAULT_NONE },

            { FIELD_MINIMAL,    0,   "minimal",        "Minimum brightness, below which"
                                                        " there will only be a shutdown.",                   DEFAULT_MINIMAL },

            { FIELD_NUM_LEVELS, 0,   "num-levels",     "The number of levels for which "
                                                        "the brightness varies from a minimum to a maximum", DEFAULT_NUM_LEVELS },

            { FIELD_TRANSITION, 0,   "transition",     "Transition period, for which changes "
                                                        "in the brightness level will be applied",           DEFAULT_TRANSITION },

            { FIELD_DEVNAME,    0,   "devname",        "Choose force backlight devices.",                   DEFAULT_NONE },

            { FIELD_SAVED,      0,   "saved",          "Request the last saved value.",                     DEFAULT_NONE },

            { FIELD_LIST,       0,   "list",           "Request the list of backlight devices.",            DEFAULT_NONE },

            { FIELD_STUB,       0,   "hibernate",      "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_STUB,       0,   "suspend",        "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_STUB,       0,   "pre",            "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_STUB,       0,   "suspend_hybrid", "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_STUB,       0,   "thaw",           "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_STUB,       0,   "resume",         "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_STUB,       0,   "post",           "This is a stub for compatibility.",                 DEFAULT_NONE },

            { FIELD_NONE,       0, null, null,                                                              DEFAULT_NONE }
    };

    return ops;
}


type_t const* __types (void)
{
    static type_t tp[] = { MAKE(MKTYPES) };

    return tp;
}

#define APPNAME "backlight"
#define DEFDIR  "/var/lib/"
default_value_t const* __defaults (void)
{
    static default_value_t defs[] = {
            { .v_str = DEFDIR APPNAME },
            { .v_str = APPNAME ".socket" },
            { .v_str = APPNAME ".pid" },
            { .v_str = APPNAME ".conf" },
            { .v_int = 2000 },
            { .v_int = 20 },
            { .v_int = 100 },
            { .v_str = null }
    };
    return defs;
}
#undef APPNAME
#undef DEFDIR

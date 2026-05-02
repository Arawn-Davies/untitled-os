/*
 * shell_help.c -- help and version built-in commands.
 *
 * 'help' redirects to 'lsman'.  Full command reference lives in
 * shell_cmd_man.c so it stays in one place.
 */

#include "shell_priv.h"
#include <kernel/tty.h>

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    t_writestring("Type 'lsman' for a list of commands.\n");
    t_writestring("Type 'man <cmd>' for details on a specific command.\n");
}

static void cmd_version(int argc, char **argv)
{
    (void)argc; (void)argv;
    t_writestring("Makar -- version " SHELL_VERSION
                  ", build: " BUILD_DATE " " BUILD_TIME "\n");
    t_writestring("The GCC/C++ sibling of Medli\n");
    t_writestring(COPYRIGHT "\n");
    t_writestring("Released under the BSD-3 Clause Clear license\n");
}

const shell_cmd_entry_t help_cmds[] = {
    { "help",    cmd_help    },
    { "version", cmd_version },
    { NULL, NULL }
};

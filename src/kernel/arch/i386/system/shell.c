/*
 * shell.c -- kernel REPL core: line input, tokenisation, and command dispatch.
 *
 * Provides:
 *   shell_readline() - echoes input, handles backspace, ends on Enter
 *   shell_run()      - infinite prompt loop ("makar-sh> ")
 *
 * Command implementations live in shell_cmds.c and shell_help.c.
 */

#include "shell_priv.h"

#include <kernel/shell.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/ktest.h>

#include <string.h>

/* ---------------------------------------------------------------------------
 * shell_readline – read a line from the PS/2 keyboard into buf.
 *
 * Echoes every printable character to the VGA terminal.  Handles:
 *   '\b' – erase the previous character (if any).
 *   '\n' – end of line.
 * Always NUL-terminates buf.  Reads at most (max - 1) characters.
 * --------------------------------------------------------------------------- */
void shell_readline(char *buf, size_t max)
{
    size_t len = 0;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n' || c == '\r') {
            t_putchar('\n');
            break;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                t_backspace();
            }
            continue;
        }

        /* Accept printable ASCII; silently drop anything else. */
        if (c < 0x20 || c > 0x7E)
            continue;

        if (len < max - 1) {
            buf[len++] = c;
            t_putchar(c);
        }
    }

    buf[len] = '\0';
}

/* ---------------------------------------------------------------------------
 * shell_parse – split line in-place into at most max_args tokens.
 *
 * Tokens are separated by spaces.  Returns the number of tokens found.
 * --------------------------------------------------------------------------- */
static int shell_parse(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        /* Skip leading spaces. */
        while (*p == ' ')
            p++;

        if (!*p)
            break;

        argv[argc++] = p;

        /* Advance to the end of the token. */
        while (*p && *p != ' ')
            p++;

        if (*p)
            *p++ = '\0';
    }

    return argc;
}

/* ---------------------------------------------------------------------------
 * Command dispatch – enum + lookup table + switch/case/default.
 *
 * To add a new command: add a CMD_FOO entry to the enum, add a row to
 * cmd_table[], add a case in shell_run().
 * --------------------------------------------------------------------------- */

typedef enum {
    CMD_HELP,
    CMD_CLEAR,
    CMD_ECHO,
    CMD_MEMINFO,
    CMD_UPTIME,
    CMD_VERSION,
    CMD_TASKS,
    CMD_LSDISKS,
    CMD_LSPART,
    CMD_MKPART,
    CMD_READSECTOR,
    CMD_SETMODE,
    CMD_SHUTDOWN,
    CMD_REBOOT,
    CMD_KTEST,
    /* FAT32 commands */
    CMD_MOUNT,
    CMD_UMOUNT,
    CMD_LS,
    CMD_CAT,
    CMD_CD,
    CMD_MKDIR,
    CMD_MKFS,
    CMD_UNKNOWN,
} shell_cmd_t;

typedef struct {
    const char *name;
    shell_cmd_t id;
} cmd_entry_t;

static const cmd_entry_t cmd_table[] = {
    { "help",       CMD_HELP       },
    { "clear",      CMD_CLEAR      },
    { "echo",       CMD_ECHO       },
    { "meminfo",    CMD_MEMINFO    },
    { "uptime",     CMD_UPTIME     },
    { "version",    CMD_VERSION    },
    { "tasks",      CMD_TASKS      },
    { "lsdisks",    CMD_LSDISKS    },
    { "lspart",     CMD_LSPART     },
    { "mkpart",     CMD_MKPART     },
    { "readsector", CMD_READSECTOR },
    { "setmode",    CMD_SETMODE    },
    { "shutdown",   CMD_SHUTDOWN   },
    { "reboot",     CMD_REBOOT     },
    { "ktest",      CMD_KTEST      },
    /* FAT32 commands */
    { "mount",      CMD_MOUNT      },
    { "umount",     CMD_UMOUNT     },
    { "ls",         CMD_LS         },
    { "cat",        CMD_CAT        },
    { "cd",         CMD_CD         },
    { "mkdir",      CMD_MKDIR      },
    { "mkfs",       CMD_MKFS       },
};

#define CMD_TABLE_SIZE ((int)(sizeof(cmd_table) / sizeof(cmd_table[0])))

static shell_cmd_t shell_lookup(const char *name)
{
    for (int i = 0; i < CMD_TABLE_SIZE; i++) {
        if (strcmp(cmd_table[i].name, name) == 0)
            return cmd_table[i].id;
    }
    return CMD_UNKNOWN;
}

/* ---------------------------------------------------------------------------
 * shell_run – infinite REPL loop.  Never returns.
 * --------------------------------------------------------------------------- */
void shell_run(void)
{
    static char buf[SHELL_MAX_INPUT];
    char *argv[SHELL_MAX_ARGS];

    t_writestring("Makar kernel shell -- " COPYRIGHT
                  " -- built " BUILD_DATE " " BUILD_TIME "\n");
    t_writestring("Type 'help' for a list of commands.\n");

    while (1) {
        t_writestring(SHELL_PROMPT);
        shell_readline(buf, SHELL_MAX_INPUT);

        int argc = shell_parse(buf, argv, SHELL_MAX_ARGS);
        if (argc == 0)
            continue;

        switch (shell_lookup(argv[0])) {
        case CMD_HELP:       cmd_help();                  break;
        case CMD_CLEAR:      cmd_clear();                 break;
        case CMD_ECHO:       cmd_echo(argc, argv);        break;
        case CMD_MEMINFO:    cmd_meminfo();               break;
        case CMD_UPTIME:     cmd_uptime();                break;
        case CMD_VERSION:    cmd_version();               break;
        case CMD_TASKS:      cmd_tasks();                 break;
        case CMD_LSDISKS:    cmd_lsdisks();               break;
        case CMD_LSPART:     cmd_lspart(argc, argv);      break;
        case CMD_MKPART:     cmd_mkpart(argc, argv);      break;
        case CMD_READSECTOR: cmd_readsector(argc, argv);  break;
        case CMD_SETMODE:    cmd_setmode(argc, argv);     break;
        case CMD_SHUTDOWN:   cmd_shutdown();              break;
        case CMD_REBOOT:     cmd_reboot();               break;
        case CMD_KTEST:      ktest_run_all();            break;
        /* FAT32 commands */
        case CMD_MOUNT:      cmd_mount(argc, argv);       break;
        case CMD_UMOUNT:     cmd_umount();                break;
        case CMD_LS:         cmd_ls(argc, argv);          break;
        case CMD_CAT:        cmd_cat(argc, argv);         break;
        case CMD_CD:         cmd_cd(argc, argv);          break;
        case CMD_MKDIR:      cmd_mkdir(argc, argv);       break;
        case CMD_MKFS:       cmd_mkfs(argc, argv);        break;
        default:
            t_writestring("Unknown command: ");
            t_writestring(argv[0]);
            t_writestring("\n");
            break;
        }
    }
}

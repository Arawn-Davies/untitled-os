/*
 * shell.c -- minimal kernel REPL over VGA + PS/2 keyboard.
 *
 * Provides:
 *   shell_readline() - echoes input, handles backspace, ends on Enter
 *   shell_run()      - infinite prompt loop ("untitled> ")
 *
 * Built-in commands: help, clear, echo, meminfo, uptime
 */

#include <kernel/shell.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/timer.h>
#include <kernel/heap.h>
#include <kernel/vesa_tty.h>

#include <string.h>
#include <stddef.h>

#define SHELL_MAX_INPUT  256
#define SHELL_MAX_ARGS   8
#define SHELL_PROMPT     "untitled> "

/* ---------------------------------------------------------------------------
 * shell_readline – read a line from the PS/2 keyboard into buf.
 *
 * Echoes every printable character to the VGA terminal.  Handles:
 *   '\b' – erase the previous character (if any).
 *   '\n' – end of line.
 * Always NUL-terminates buf.  Reads at most (max - 1) characters.
 * --------------------------------------------------------------------------- */
static void shell_readline(char *buf, size_t max)
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
 * Built-in command handlers
 * --------------------------------------------------------------------------- */

static void cmd_help(void)
{
    t_writestring("Commands:\n");
    t_writestring("  help          - list commands\n");
    t_writestring("  clear         - clear the terminal\n");
    t_writestring("  echo [args..] - print arguments\n");
    t_writestring("  meminfo       - print heap used/free\n");
    t_writestring("  uptime        - ticks since boot\n");
}

static void cmd_clear(void)
{
    terminal_initialize();
    vesa_tty_init();
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            t_putchar(' ');
        t_writestring(argv[i]);
    }
    t_putchar('\n');
}

static void cmd_meminfo(void)
{
    t_writestring("heap used: ");
    t_dec((uint32_t)heap_used());
    t_writestring(" bytes\n");
    t_writestring("heap free: ");
    t_dec((uint32_t)heap_free());
    t_writestring(" bytes\n");
}

static void cmd_uptime(void)
{
    t_writestring("uptime: ");
    t_dec(timer_get_ticks());
    t_writestring(" ticks\n");
}

/* ---------------------------------------------------------------------------
 * shell_run – infinite REPL loop.  Never returns.
 * --------------------------------------------------------------------------- */
void shell_run(void)
{
    static char buf[SHELL_MAX_INPUT];
    char *argv[SHELL_MAX_ARGS];

    t_writestring("Makar kernel shell. Type 'help' for a list of commands.\n");

    while (1) {
        t_writestring(SHELL_PROMPT);
        shell_readline(buf, SHELL_MAX_INPUT);

        int argc = shell_parse(buf, argv, SHELL_MAX_ARGS);
        if (argc == 0)
            continue;

        if (strcmp(argv[0], "help") == 0) {
            cmd_help();
        } else if (strcmp(argv[0], "clear") == 0) {
            cmd_clear();
        } else if (strcmp(argv[0], "echo") == 0) {
            cmd_echo(argc, argv);
        } else if (strcmp(argv[0], "meminfo") == 0) {
            cmd_meminfo();
        } else if (strcmp(argv[0], "uptime") == 0) {
            cmd_uptime();
        } else {
            t_writestring("Unknown command: ");
            t_writestring(argv[0]);
            t_writestring("\n");
        }
    }
}

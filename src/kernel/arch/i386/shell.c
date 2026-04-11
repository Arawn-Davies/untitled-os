/*
 * shell.c -- minimal kernel REPL over VGA + PS/2 keyboard.
 *
 * Provides:
 *   shell_readline() - echoes input, handles backspace, ends on Enter
 *   shell_run()      - infinite prompt loop ("untitled> ")
 *
 * Built-in commands: help, clear, echo, meminfo, uptime, shutdown
 */

#include <kernel/shell.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/timer.h>
#include <kernel/heap.h>
#include <kernel/vesa_tty.h>
#include <kernel/ide.h>

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
    t_writestring("  help                   - list commands\n");
    t_writestring("  clear                  - clear the terminal\n");
    t_writestring("  echo [args..]          - print arguments\n");
    t_writestring("  meminfo                - print heap used/free\n");
    t_writestring("  uptime                 - ticks since boot\n");
    t_writestring("  lsdisks                - list detected ATA drives\n");
    t_writestring("  readsector <drv> <lba> - hex-dump one sector\n");
    t_writestring("  shutdown               - halt the system\n");
}

static void cmd_clear(void)
{
    /* Both outputs are always active in parallel: tty.c writes to VGA and
       forwards every character to vesa_tty.  Reset both so they stay in sync.
       vesa_tty_init() is a no-op when no VESA framebuffer is present. */
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

static void cmd_lsdisks(void)
{
    static const char * const type_str[] = { "none", "ATA", "ATAPI" };
    int found = 0;

    for (uint8_t i = 0; i < IDE_MAX_DRIVES; i++) {
        const ide_drive_t *d = ide_get_drive(i);
        if (!d || !d->present)
            continue;

        found = 1;
        t_writestring("drive ");
        t_dec(i);
        t_writestring(": [");
        t_writestring(d->channel == 0 ? "primary" : "secondary");
        t_putchar(' ');
        t_writestring(d->drive == 0 ? "master" : "slave");
        t_writestring("] ");
        t_writestring(type_str[d->type]);
        t_writestring("  ");
        t_dec(d->size / 2048);   /* MiB: sectors * 512 / (1024*1024) */
        t_writestring(" MiB  \"");
        t_writestring(d->model);
        t_writestring("\"\n");
    }

    if (!found)
        t_writestring("No drives detected.\n");
}

/* Simple hex dump of a 512-byte buffer: 32 rows of 16 bytes. */
static void hexdump_sector(const uint8_t *buf)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int row = 0; row < 32; row++) {
        int offset = row * 16;
        /* Offset */
        t_putchar(hex[(offset >> 8) & 0xF]);
        t_putchar(hex[(offset >> 4) & 0xF]);
        t_putchar(hex[(offset     ) & 0xF]);
        t_putchar('0');
        t_writestring(":  ");

        for (int col = 0; col < 16; col++) {
            uint8_t b = buf[offset + col];
            t_putchar(hex[b >> 4]);
            t_putchar(hex[b & 0xF]);
            t_putchar(' ');
        }
        t_putchar('\n');
    }
}

/* Parse a simple decimal or 0x-prefixed hex number from a string. */
static uint32_t parse_uint(const char *s)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        uint32_t v = 0;
        s += 2;
        while (*s) {
            char c = *s++;
            if (c >= '0' && c <= '9')      v = v * 16 + (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f') v = v * 16 + (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = v * 16 + (uint32_t)(c - 'A' + 10);
            else break;
        }
        return v;
    }
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (uint32_t)(*s++ - '0');
    return v;
}

static uint8_t sector_buf[512];

static void cmd_readsector(int argc, char **argv)
{
    if (argc < 3) {
        t_writestring("Usage: readsector <drive> <lba>\n");
        return;
    }

    uint8_t  drive = (uint8_t)parse_uint(argv[1]);
    uint32_t lba   = parse_uint(argv[2]);

    const ide_drive_t *d = ide_get_drive(drive);
    if (!d || !d->present) {
        t_writestring("Error: drive not present.\n");
        return;
    }
    if (d->type != IDE_TYPE_ATA) {
        t_writestring("Error: drive is not ATA (read not supported).\n");
        return;
    }

    int err = ide_read_sectors(drive, lba, 1, sector_buf);
    if (err) {
        t_writestring("Read error: ");
        t_dec((uint32_t)err);
        t_putchar('\n');
        return;
    }

    t_writestring("Sector ");
    t_dec(lba);
    t_writestring(" of drive ");
    t_dec(drive);
    t_writestring(":\n");
    hexdump_sector(sector_buf);
}

static void cmd_shutdown(void)
{
    t_writestring("System halted. It is now safe to turn off your computer.\n");
    asm volatile ("cli");
    for (;;)
        asm volatile ("hlt");
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
        } else if (strcmp(argv[0], "lsdisks") == 0) {
            cmd_lsdisks();
        } else if (strcmp(argv[0], "readsector") == 0) {
            cmd_readsector(argc, argv);
        } else if (strcmp(argv[0], "shutdown") == 0) {
            cmd_shutdown();
        } else {
            t_writestring("Unknown command: ");
            t_writestring(argv[0]);
            t_writestring("\n");
        }
    }
}

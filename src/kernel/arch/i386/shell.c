/*
 * shell.c -- minimal kernel REPL over VGA + PS/2 keyboard.
 *
 * Provides:
 *   shell_readline() - echoes input, handles backspace, ends on Enter
 *   shell_run()      - infinite prompt loop ("makar> ")
 *
 * Built-in commands:
 *   help, clear, echo, meminfo, uptime, shutdown
 *   disk list                   - list detected ATA drives
 *   disk info  <n>              - drive details + MBR partition table
 *   disk read  <n> <lba>        - hex-dump one sector
 *   diskutil                    - interactive Medli-style disk utility
 */

#include <kernel/shell.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/timer.h>
#include <kernel/heap.h>
#include <kernel/vesa_tty.h>
#include <kernel/ata.h>
#include <kernel/mbr.h>

#include <string.h>
#include <stddef.h>

#define SHELL_MAX_INPUT  256
#define SHELL_MAX_ARGS   8
#define SHELL_PROMPT     "makar> "

/* ---------------------------------------------------------------------------
 * shell_readline – read a line from the PS/2 keyboard into buf.
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
 * --------------------------------------------------------------------------- */
static int shell_parse(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = '\0';
    }

    return argc;
}

/* ---------------------------------------------------------------------------
 * Utility: parse a decimal string to uint32_t.  Returns 0 if s is NULL/empty.
 * --------------------------------------------------------------------------- */
static uint32_t parse_uint(const char *s)
{
    if (!s || !*s)
        return 0;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (uint32_t)(*s++ - '0');
    return v;
}

/* ---------------------------------------------------------------------------
 * Utility: print a two-digit hex byte (no "0x" prefix).
 * --------------------------------------------------------------------------- */
static void print_hex8(uint8_t b)
{
    static const char h[] = "0123456789ABCDEF";
    t_putchar(h[(b >> 4) & 0xF]);
    t_putchar(h[b & 0xF]);
}

/* ---------------------------------------------------------------------------
 * Built-in: help
 * --------------------------------------------------------------------------- */
static void cmd_help(void)
{
    t_writestring("Commands:\n");
    t_writestring("  help               - list commands\n");
    t_writestring("  clear              - clear the terminal\n");
    t_writestring("  echo [args..]      - print arguments\n");
    t_writestring("  meminfo            - heap used/free\n");
    t_writestring("  uptime             - ticks since boot\n");
    t_writestring("  shutdown           - halt the system\n");
    t_writestring("  disk list          - list ATA drives\n");
    t_writestring("  disk info  <n>     - drive info + partition table\n");
    t_writestring("  disk read  <n> <lba> - hex-dump one sector\n");
    t_writestring("  diskutil           - interactive disk utility\n");
}

/* ---------------------------------------------------------------------------
 * Built-in: clear
 * --------------------------------------------------------------------------- */
static void cmd_clear(void)
{
    terminal_initialize();
    vesa_tty_init();
}

/* ---------------------------------------------------------------------------
 * Built-in: echo
 * --------------------------------------------------------------------------- */
static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            t_putchar(' ');
        t_writestring(argv[i]);
    }
    t_putchar('\n');
}

/* ---------------------------------------------------------------------------
 * Built-in: meminfo
 * --------------------------------------------------------------------------- */
static void cmd_meminfo(void)
{
    t_writestring("heap used: ");
    t_dec((uint32_t)heap_used());
    t_writestring(" bytes\n");
    t_writestring("heap free: ");
    t_dec((uint32_t)heap_free());
    t_writestring(" bytes\n");
}

/* ---------------------------------------------------------------------------
 * Built-in: uptime
 * --------------------------------------------------------------------------- */
static void cmd_uptime(void)
{
    t_writestring("uptime: ");
    t_dec(timer_get_ticks());
    t_writestring(" ticks\n");
}

/* ---------------------------------------------------------------------------
 * Built-in: shutdown
 * --------------------------------------------------------------------------- */
static void cmd_shutdown(void)
{
    t_writestring("System halted. It is now safe to turn off your computer.\n");
    asm volatile ("cli");
    for (;;)
        asm volatile ("hlt");
}

/* ---------------------------------------------------------------------------
 * disk list — enumerate detected ATA drives (mirrors MFSU ListDisks).
 * --------------------------------------------------------------------------- */
static void disk_list(void)
{
    int found = 0;
    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        const ata_drive_t *drv = ata_get_drive(i);
        if (!drv || !drv->present)
            continue;
        found++;
        t_writestring("  Drive ");
        t_dec((uint32_t)i);
        t_writestring(": ");
        t_writestring(drv->model[0] ? drv->model : "(unknown)");
        t_writestring("  ");
        t_dec(drv->sectors / 2048);
        t_writestring(" MiB (");
        t_dec(drv->sectors);
        t_writestring(" sectors)\n");
    }
    if (!found)
        t_writestring("  No ATA drives detected.\n");
}

/* ---------------------------------------------------------------------------
 * disk info <n> — drive details + MBR partition table (mirrors MFSU DiskPartInfo).
 * --------------------------------------------------------------------------- */
static void disk_info(int drive_idx)
{
    const ata_drive_t *drv = ata_get_drive(drive_idx);
    if (!drv || !drv->present) {
        t_writestring("disk: no drive at index ");
        t_dec((uint32_t)drive_idx);
        t_putchar('\n');
        return;
    }

    mbr_t mbr;
    int rc = mbr_read(drive_idx, &mbr);
    if (rc == -1) {
        t_writestring("disk: read error on drive ");
        t_dec((uint32_t)drive_idx);
        t_putchar('\n');
        return;
    }
    mbr_print(drive_idx, &mbr);
}

/* ---------------------------------------------------------------------------
 * disk read <n> <lba> — hex + ASCII dump of one sector.
 * --------------------------------------------------------------------------- */
static void disk_read_sector(int drive_idx, uint32_t lba)
{
    const ata_drive_t *drv = ata_get_drive(drive_idx);
    if (!drv || !drv->present) {
        t_writestring("disk: no drive at index ");
        t_dec((uint32_t)drive_idx);
        t_putchar('\n');
        return;
    }

    uint8_t buf[ATA_SECTOR_SIZE];
    if (ata_read(drive_idx, lba, 1, buf) < 0) {
        t_writestring("disk: read error (drive=");
        t_dec((uint32_t)drive_idx);
        t_writestring(" lba=");
        t_dec(lba);
        t_writestring(")\n");
        return;
    }

    /* Print 32 rows × 16 bytes in classic hex-dump style. */
    for (int row = 0; row < 32; row++) {
        /* Offset */
        print_hex8((uint8_t)((row * 16) >> 8));
        print_hex8((uint8_t)(row * 16));
        t_writestring(": ");

        /* Hex bytes (two groups of 8). */
        for (int col = 0; col < 16; col++) {
            print_hex8(buf[row * 16 + col]);
            t_putchar(' ');
            if (col == 7)
                t_putchar(' ');
        }

        /* ASCII column. */
        t_writestring(" |");
        for (int col = 0; col < 16; col++) {
            uint8_t c = buf[row * 16 + col];
            t_putchar((c >= 0x20 && c < 0x7F) ? (char)c : '.');
        }
        t_writestring("|\n");
    }
}

/* ---------------------------------------------------------------------------
 * cmd_disk — dispatch `disk <subcommand> [args...]`.
 * --------------------------------------------------------------------------- */
static void cmd_disk(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring("Usage: disk list | disk info <n> | disk read <n> <lba>\n");
        return;
    }

    if (strcmp(argv[1], "list") == 0) {
        disk_list();
    } else if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) {
            t_writestring("disk info: missing drive index\n");
            return;
        }
        disk_info((int)parse_uint(argv[2]));
    } else if (strcmp(argv[1], "read") == 0) {
        if (argc < 4) {
            t_writestring("disk read: usage: disk read <drive> <lba>\n");
            return;
        }
        disk_read_sector((int)parse_uint(argv[2]), parse_uint(argv[3]));
    } else {
        t_writestring("disk: unknown subcommand '");
        t_writestring(argv[1]);
        t_writestring("'\n");
    }
}

/* ---------------------------------------------------------------------------
 * diskutil — interactive Medli MFSU-style disk utility.
 *
 * Menu:
 *   1) List drives
 *   2) Select drive
 *   3) Partition info
 *   4) Create partitions
 *   5) Exit
 * --------------------------------------------------------------------------- */

/* State shared across diskutil sub-functions. */
static int diskutil_selected = -1; /* -1 = none selected */

/* Print the drive list, marking the selected drive with '*'. */
static void diskutil_list_drives(void)
{
    int found = 0;
    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        const ata_drive_t *drv = ata_get_drive(i);
        if (!drv || !drv->present)
            continue;
        found++;
        if (i == diskutil_selected)
            t_writestring("  * ");
        else
            t_writestring("    ");
        t_dec((uint32_t)i);
        t_writestring(") ");
        t_writestring(drv->model[0] ? drv->model : "(unknown)");
        t_writestring("  ");
        t_dec(drv->sectors / 2048);
        t_writestring(" MiB\n");
    }
    if (!found)
        t_writestring("  No ATA drives detected.\n");
}

/* Prompt the user to select a drive by number. */
static void diskutil_select_drive(void)
{
    t_writestring("Available drives:\n");
    diskutil_list_drives();
    t_writestring("Enter drive number: ");

    static char buf[8];
    shell_readline(buf, sizeof(buf));
    int n = (int)parse_uint(buf);

    const ata_drive_t *drv = ata_get_drive(n);
    if (!drv || !drv->present) {
        t_writestring("  Invalid selection.\n");
        return;
    }
    diskutil_selected = n;
    t_writestring("  Drive ");
    t_dec((uint32_t)n);
    t_writestring(" selected.\n");
}

/* Show partition info for the selected drive. */
static void diskutil_part_info(void)
{
    if (diskutil_selected < 0) {
        t_writestring("  No drive selected.\n");
        return;
    }
    disk_info(diskutil_selected);
}

/*
 * Create (or overwrite) up to 4 primary partitions on the selected drive.
 * Mirrors Medli MFSU CreatePartitions():
 *   - ask how many partitions (1–4)
 *   - for each: ask block count
 *   - write updated partition table to the MBR
 */
static void diskutil_create_partitions(void)
{
    if (diskutil_selected < 0) {
        t_writestring("  No drive selected.\n");
        return;
    }

    const ata_drive_t *drv = ata_get_drive(diskutil_selected);
    if (!drv || !drv->present) {
        t_writestring("  Selected drive is no longer available.\n");
        return;
    }

    /* Total usable sectors (reserve sector 0 for the MBR). */
    uint32_t total_usable = drv->sectors > 0 ? drv->sectors - 1 : 0;

    /* Ask for number of partitions. */
    t_writestring("  How many primary partitions? (1-4): ");
    static char buf[8];
    shell_readline(buf, sizeof(buf));
    int nparts = (int)parse_uint(buf);
    if (nparts < 1 || nparts > 4) {
        t_writestring("  Invalid count.\n");
        return;
    }

    uint32_t starts[4]  = {1, 0, 0, 0}; /* first starts at sector 1 */
    uint32_t counts[4]  = {0, 0, 0, 0};
    uint32_t remaining  = total_usable;

    for (int i = 0; i < nparts; i++) {
        /* How many sectors still need to be reserved for subsequent partitions? */
        uint32_t min_for_rest = (uint32_t)(nparts - i - 1);
        uint32_t max_sectors  = (remaining > min_for_rest)
                                ? remaining - min_for_rest : 0;

        t_writestring("  Partition ");
        t_dec((uint32_t)(i + 1));
        t_writestring(" — available sectors: ");
        t_dec(max_sectors);
        t_writestring("  (");
        t_dec(max_sectors / 2048);
        t_writestring(" MiB)\n");
        t_writestring("  Sectors for this partition: ");

        shell_readline(buf, sizeof(buf));
        uint32_t n = parse_uint(buf);

        if (n == 0 || n > max_sectors) {
            t_writestring("  Value out of range; aborting.\n");
            return;
        }

        counts[i] = n;
        if (i + 1 < nparts)
            starts[i + 1] = starts[i] + n;
        remaining -= n;
    }

    /* Build the new MBR by reading the existing one and patching it. */
    mbr_t mbr;
    /* Ignore return value: we can create a fresh table on a blank disk. */
    mbr_read(diskutil_selected, &mbr);

    for (int i = 0; i < 4; i++) {
        if (i < nparts) {
            mbr.parts[i].status       = 0x00;
            mbr.parts[i].type         = 0xFA; /* MDFS — shared Makar/Medli ID */
            mbr.parts[i].lba_start    = starts[i];
            mbr.parts[i].sector_count = counts[i];
            /* Clear CHS fields — not used in LBA mode. */
            mbr.parts[i].chs_first[0] = 0;
            mbr.parts[i].chs_first[1] = 0;
            mbr.parts[i].chs_first[2] = 0;
            mbr.parts[i].chs_last[0]  = 0;
            mbr.parts[i].chs_last[1]  = 0;
            mbr.parts[i].chs_last[2]  = 0;
        } else {
            /* Zero out unused entries. */
            mbr.parts[i].status       = 0;
            mbr.parts[i].type         = 0;
            mbr.parts[i].lba_start    = 0;
            mbr.parts[i].sector_count = 0;
        }
    }
    mbr.valid = 1;

    if (mbr_write(diskutil_selected, &mbr) < 0) {
        t_writestring("  Error: could not write MBR.\n");
        return;
    }

    t_writestring("  Partition table written. New layout:\n");
    mbr_print(diskutil_selected, &mbr);
}

/* ---------------------------------------------------------------------------
 * cmd_diskutil — the top-level interactive loop.
 * --------------------------------------------------------------------------- */
static void cmd_diskutil(void)
{
    static char buf[SHELL_MAX_INPUT];
    char *argv[SHELL_MAX_ARGS];

    t_writestring("=== Makar Disk Utility ===\n");
    diskutil_selected = -1;

    int running = 1;
    while (running) {
        t_writestring("\n1) List drives  2) Select drive  3) Partition info\n");
        t_writestring("4) Create partitions  5) Exit\n> ");

        shell_readline(buf, sizeof(buf));

        /* Accept numeric option or keyword. */
        int argc = shell_parse(buf, argv, SHELL_MAX_ARGS);
        if (argc == 0)
            continue;

        const char *opt = argv[0];

        if (strcmp(opt, "1") == 0 || strcmp(opt, "list") == 0) {
            diskutil_list_drives();
        } else if (strcmp(opt, "2") == 0 || strcmp(opt, "select") == 0) {
            diskutil_select_drive();
        } else if (strcmp(opt, "3") == 0 || strcmp(opt, "parts") == 0) {
            diskutil_part_info();
        } else if (strcmp(opt, "4") == 0 || strcmp(opt, "create") == 0) {
            diskutil_create_partitions();
        } else if (strcmp(opt, "5") == 0 ||
                   strcmp(opt, "exit") == 0 ||
                   strcmp(opt, "quit") == 0) {
            running = 0;
        } else {
            t_writestring("Unknown option: ");
            t_writestring(opt);
            t_putchar('\n');
        }
    }

    t_writestring("Exiting disk utility.\n");
}

/* ---------------------------------------------------------------------------
 * shell_run — infinite REPL loop.  Never returns.
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
        } else if (strcmp(argv[0], "shutdown") == 0) {
            cmd_shutdown();
        } else if (strcmp(argv[0], "disk") == 0) {
            cmd_disk(argc, argv);
        } else if (strcmp(argv[0], "diskutil") == 0) {
            cmd_diskutil();
        } else {
            t_writestring("Unknown command: ");
            t_writestring(argv[0]);
            t_writestring("\n");
        }
    }
}

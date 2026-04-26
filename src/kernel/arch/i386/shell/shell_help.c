/*
 * shell_help.c -- help and version built-in commands.
 */

#include "shell_priv.h"

#include <kernel/tty.h>
#include <kernel/keyboard.h>

/* ---------------------------------------------------------------------------
 * Simple line-level pager for cmd_help.
 *
 * pager_start() must be called before the first pager_line() call.
 * pager_line() prints one line (including its trailing '\n') and pauses when
 * the screen is full, waiting for a keypress before continuing.
 * --------------------------------------------------------------------------- */

static int s_pager_line;
static int s_pager_size;

static void pager_start(void)
{
    s_pager_line = 0;
    /* Reserve 3 rows: the "-- more --" prompt, a blank, and the shell prompt. */
    s_pager_size = t_get_rows() - 3;
    if (s_pager_size < 5)
        s_pager_size = 5;
}

static void pager_line(const char *s)
{
    if (s_pager_line >= s_pager_size) {
        t_writestring("-- more -- (any key)");
        keyboard_getchar();
        t_putchar('\n');
        s_pager_line = 0;
    }
    t_writestring(s);
    s_pager_line++;
}

/* ---------------------------------------------------------------------------
 * cmd_help
 * --------------------------------------------------------------------------- */

void cmd_help(void)
{
    pager_start();
    pager_line(COPYRIGHT "\n");
    pager_line("Commands:\n");
    pager_line("  help                         - list commands\n");
    pager_line("  clear                        - clear the terminal\n");
    pager_line("  echo [args..]                - print arguments\n");
    pager_line("  meminfo                      - print heap used/free\n");
    pager_line("  uptime                       - ticks since boot\n");
    pager_line("  version                      - show build info and copyright\n");
    pager_line("  tasks                        - list kernel tasks\n");
    pager_line("  lsdisks                      - list detected ATA drives\n");
    pager_line("  lspart <drv>                 - list partitions (MBR or GPT)\n");
    pager_line("  mkpart <drv> <mbr|gpt>       - create a partition table\n");
    pager_line("  readsector <drv> <lba>       - hex-dump one sector\n");
    pager_line("  setmode <25|50>              - switch between 80x25 and 80x50\n");
    pager_line("  shutdown                     - power off the system (ACPI S5)\n");
    pager_line("  reboot                       - reboot the system (ACPI/KBC)\n");
    pager_line("  ktest                        - run in-kernel unit tests\n");
    pager_line("Filesystem (universal VFS: /hd for HDD, /cdrom for CD-ROM):\n");
    pager_line("  mkfs <drv> <part#>           - format partition as FAT32\n");
    pager_line("  mount <drv> <part#>          - mount FAT32 partition at /hd/\n");
    pager_line("  umount                       - unmount the FAT32 volume\n");
    pager_line("  ls [path]                    - list directory (/hd/.. or /cdrom/..)\n");
    pager_line("  cd <path>                    - change directory\n");
    pager_line("  cat <file>                   - print file contents\n");
    pager_line("  mkdir <path>                 - create directory (FAT32 only)\n");
    pager_line("  isols <drv> [path]           - list directory on an ISO9660 CD-ROM\n");
    pager_line("Installer:\n");
    pager_line("  install                      - install Makar OS to a hard drive\n");
    pager_line("Boot:\n");
    pager_line("  chainload <drv> [lba]        - boot a sector from disk (lba=0: MBR/GRUB)\n");
    pager_line("Development:\n");
    pager_line("  ring3test                    - spawn a ring-3 smoke test process\n");
    pager_line("  exec <path>                  - load and run an ELF32 executable\n");
}

void cmd_version(void)
{
    t_writestring("Makar -- version " SHELL_VERSION
                  ", build: " BUILD_DATE " " BUILD_TIME "\n");
    t_writestring("The GCC/C++ sibling of Medli\n");
    t_writestring(COPYRIGHT "\n");
    t_writestring("Released under the BSD-3 Clause Clear license\n");
}

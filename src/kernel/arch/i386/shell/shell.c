/*
 * shell.c -- kernel REPL core: line input, tokenisation, and command dispatch.
 *
 * Provides:
 *   shell_readline() - echoes input, handles backspace, ends on Enter
 *   shell_run()      - infinite prompt loop (Medli-style UX)
 *
 * Command implementations live in shell_cmds.c and shell_help.c.
 */

#include "shell_priv.h"

#include <kernel/shell.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/vga.h>
#include <kernel/vesa.h>
#include <kernel/vesa_tty.h>
#include <kernel/serial.h>
#include <kernel/vfs.h>
#include <kernel/ktest.h>

#include <string.h>

/* ---------------------------------------------------------------------------
 * Command history ring-buffer (SHELL_HISTORY_SIZE most-recent entries).
 * --------------------------------------------------------------------------- */
#define SHELL_HISTORY_SIZE 16

static char s_history[SHELL_HISTORY_SIZE][SHELL_MAX_INPUT];
static int  s_hist_count = 0;  /* valid entries (capped at SHELL_HISTORY_SIZE) */
static int  s_hist_head  = 0;  /* index of the next write slot                 */

static void history_push(const char *line)
{
    if (!line || !*line)
        return;
    strncpy(s_history[s_hist_head], line, SHELL_MAX_INPUT - 1);
    s_history[s_hist_head][SHELL_MAX_INPUT - 1] = '\0';
    s_hist_head = (s_hist_head + 1) % SHELL_HISTORY_SIZE;
    if (s_hist_count < SHELL_HISTORY_SIZE)
        s_hist_count++;
}

/* ago=0 → most-recent entry; returns NULL when out of range. */
static const char *history_get(int ago)
{
    if (ago < 0 || ago >= s_hist_count)
        return NULL;
    int idx = (s_hist_head - 1 - ago + SHELL_HISTORY_SIZE) % SHELL_HISTORY_SIZE;
    return s_history[idx];
}

/* ---------------------------------------------------------------------------
 * readline_redraw – repaint the input line in-place and position cursors.
 *
 * VGA uses vga_start_col/row (always 80 cols).
 * VESA uses vesa_start_col/row (tty_cols wide, which differs from VGA_WIDTH).
 * Passing separate VESA coords avoids the t_row=0 desync that occurs when
 * VESA is active and the VGA row counter wraps.
 * --------------------------------------------------------------------------- */
static void readline_redraw(const char *buf, size_t len, size_t cur,
                             size_t vga_start_col, size_t vga_start_row,
                             uint32_t vesa_start_col, uint32_t vesa_start_row)
{
    uint32_t vcols = vesa_tty_is_ready() ? vesa_tty_get_cols() : VGA_WIDTH;

    for (size_t i = 0; i <= len; i++) {
        char ch = (i < len) ? buf[i] : ' '; /* trailing space erases deletions */

        size_t vga_lp  = vga_start_col + i;
        size_t vga_row = vga_start_row + vga_lp / VGA_WIDTH;
        size_t vga_col = vga_lp % VGA_WIDTH;
        VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] = make_vgaentry(ch, SHELL_COLOR_VGA);

        if (vesa_tty_is_ready()) {
            uint32_t vlp = vesa_start_col + (uint32_t)i;
            vesa_tty_put_at(ch, vlp % vcols, vesa_start_row + vlp / vcols);
        }
    }

    /* VGA hardware cursor */
    size_t cp = vga_start_col + cur;
    update_cursor(vga_start_row + cp / VGA_WIDTH, cp % VGA_WIDTH);

    /* VESA internal cursor */
    if (vesa_tty_is_ready()) {
        uint32_t vcp = vesa_start_col + (uint32_t)cur;
        vesa_tty_set_cursor(vcp % vcols, vesa_start_row + vcp / vcols);
    }
}

/* ---------------------------------------------------------------------------
 * shell_readline – read a line from the PS/2 keyboard into buf.
 *
 * Handles:
 *   Printable       – insert at cursor (inline editing).
 *   '\b'            – delete char before cursor.
 *   KEY_ARROW_LEFT  – move cursor left within the line.
 *   KEY_ARROW_RIGHT – move cursor right within the line.
 *   KEY_ARROW_UP    – recall older history entry.
 *   KEY_ARROW_DOWN  – recall newer entry (or restore working line).
 *   '\n' / '\r'     – end of line.
 * Always NUL-terminates buf.  Reads at most (max - 1) characters.
 * --------------------------------------------------------------------------- */
void shell_readline(char *buf, size_t max)
{
    size_t len      = 0;
    size_t cur      = 0;             /* cursor position within buf            */
    int    hist_pos = -1;            /* -1 = not in history-navigation mode   */
    char   work[SHELL_MAX_INPUT];    /* in-progress line saved on first ↑     */

    buf[0]  = '\0';
    work[0] = '\0';

    /* Capture where the prompt ended.  VGA uses t_column/t_row (wraps at
     * VGA_WIDTH and resets t_row=0 when VESA is active).  VESA tracks its
     * own cursor which must be read separately to avoid the t_row=0 desync. */
    size_t   rl_col      = t_column;
    size_t   rl_row      = t_row;
    uint32_t vesa_rl_col = vesa_tty_is_ready() ? vesa_tty_get_col() : (uint32_t)t_column;
    uint32_t vesa_rl_row = vesa_tty_is_ready() ? vesa_tty_get_row() : (uint32_t)t_row;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n' || c == '\r') {
            /* Sync VGA tty state to end of line, then emit newline. */
            size_t end = rl_col + len;
            t_row      = rl_row + end / VGA_WIDTH;
            t_column   = end % VGA_WIDTH;
            if (vesa_tty_is_ready()) {
                uint32_t vcols = vesa_tty_get_cols();
                uint32_t vend  = vesa_rl_col + (uint32_t)len;
                vesa_tty_set_cursor(vend % vcols, vesa_rl_row + vend / vcols);
            }
            t_putchar('\n');  /* also writes '\n' to serial via tty.c */
            break;
        }

        if (c == '\b') {
            if (cur > 0) {
                for (size_t i = cur - 1; i < len - 1; i++)
                    buf[i] = buf[i + 1];
                len--;
                cur--;
                buf[len] = '\0';
                Serial_WriteChar('\b');
                readline_redraw(buf, len, cur,
                                rl_col, rl_row, vesa_rl_col, vesa_rl_row);
            }
            hist_pos = -1;
            continue;
        }

        /* ---- Inline cursor movement ---- */
        if (c == KEY_ARROW_LEFT) {
            if (cur > 0) {
                cur--;
                size_t cp = rl_col + cur;
                update_cursor(rl_row + cp / VGA_WIDTH, cp % VGA_WIDTH);
                if (vesa_tty_is_ready()) {
                    uint32_t vcols = vesa_tty_get_cols();
                    uint32_t vcp   = vesa_rl_col + (uint32_t)cur;
                    vesa_tty_set_cursor(vcp % vcols, vesa_rl_row + vcp / vcols);
                }
            }
            continue;
        }

        if (c == KEY_ARROW_RIGHT) {
            if (cur < len) {
                cur++;
                size_t cp = rl_col + cur;
                update_cursor(rl_row + cp / VGA_WIDTH, cp % VGA_WIDTH);
                if (vesa_tty_is_ready()) {
                    uint32_t vcols = vesa_tty_get_cols();
                    uint32_t vcp   = vesa_rl_col + (uint32_t)cur;
                    vesa_tty_set_cursor(vcp % vcols, vesa_rl_row + vcp / vcols);
                }
            }
            continue;
        }

        /* ---- History navigation ---- */
        if (c == KEY_ARROW_UP || c == KEY_ARROW_DOWN) {
            int new_pos;

            if (c == KEY_ARROW_UP) {
                if (hist_pos < 0) {
                    strncpy(work, buf, max - 1);
                    work[max - 1] = '\0';
                }
                new_pos = (hist_pos < 0) ? 0 : hist_pos + 1;
                if (new_pos >= s_hist_count)
                    new_pos = s_hist_count - 1;
            } else {
                new_pos = hist_pos - 1;
            }

            const char *entry;
            if (new_pos < 0) {
                entry    = work;
                hist_pos = -1;
            } else {
                entry = history_get(new_pos);
                if (!entry) continue;
                hist_pos = new_pos;
            }

            strncpy(buf, entry, max - 1);
            buf[max - 1] = '\0';
            len = strlen(buf);
            cur = len;
            readline_redraw(buf, len, cur,
                            rl_col, rl_row, vesa_rl_col, vesa_rl_row);
            continue;
        }

        /* Accept printable ASCII; silently drop everything else. */
        if (c < 0x20 || c > 0x7E)
            continue;

        hist_pos = -1;

        if (len < max - 1) {
            for (size_t i = len; i > cur; i--)
                buf[i] = buf[i - 1];
            buf[cur] = c;
            len++;
            cur++;
            buf[len] = '\0';
            Serial_WriteChar(c);
            readline_redraw(buf, len, cur,
                            rl_col, rl_row, vesa_rl_col, vesa_rl_row);
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
    /* ISO9660 commands */
    CMD_ISOLS,
    /* Installer */
    CMD_INSTALL,
    CMD_VICS,
    CMD_EJECT,
    CMD_RING3TEST,
    CMD_PANIC,
    CMD_CHAINLOAD,
    CMD_EXEC,
    /* File I/O */
    CMD_WRITE,
    CMD_TOUCH,
    CMD_CP,
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
    /* ISO9660 commands */
    { "isols",      CMD_ISOLS      },
    /* Installer */
    { "install",    CMD_INSTALL    },
    { "vics",       CMD_VICS       },
    { "eject",      CMD_EJECT      },
    { "ring3test",  CMD_RING3TEST  },
    { "panic",      CMD_PANIC      },
    { "chainload",  CMD_CHAINLOAD  },
    { "exec",       CMD_EXEC       },
    /* File I/O */
    { "write",      CMD_WRITE      },
    { "touch",      CMD_TOUCH      },
    { "cp",         CMD_CP         },
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
 * shell_print_prompt – print the interactive prompt showing the VFS CWD.
 *
 * Format:  root@makar /hd/boot~>
 * --------------------------------------------------------------------------- */
static void shell_print_prompt(void)
{
    t_writestring(SHELL_USERNAME "@" SHELL_HOSTNAME " ");
    t_writestring(vfs_getcwd());
    t_writestring("~> ");
}

/* ---------------------------------------------------------------------------
 * shell_run – infinite REPL loop.  Never returns.
 * --------------------------------------------------------------------------- */
void shell_run(void)
{
    static char buf[SHELL_MAX_INPUT];
    char *argv[SHELL_MAX_ARGS];

    terminal_set_colorscheme(SHELL_COLOR_VGA);
    if (vesa_tty_is_ready()) {
        vesa_tty_setcolor(SHELL_FG_RGB, SHELL_BG_RGB);
        vesa_tty_clear();

#ifndef TEST_MODE
        /* Splash screen: blit the logo centred, prompt for a keypress, then
         * clear back to the normal shell background before printing the banner. */
        vesa_blit_logo(SHELL_FG_RGB, SHELL_BG_RGB);

        /* "Press any key to continue..." centred on the bottom quarter. */
        static const char *prompt_str = "Press any key to continue...";
        uint32_t cols      = vesa_tty_get_cols();
        uint32_t rows      = vesa_tty_get_rows();
        uint32_t prompt_len = 28; /* strlen(prompt_str) */
        uint32_t prompt_col = (cols > prompt_len) ? (cols - prompt_len) / 2 : 0;
        uint32_t prompt_row = rows - 3;
        for (uint32_t i = 0; i < prompt_len; i++)
            vesa_tty_put_at(prompt_str[i], prompt_col + i, prompt_row);

        keyboard_getchar();
        vesa_tty_clear();
#endif
    }

    t_writestring("Makar -- version " SHELL_VERSION
                  ", build: " BUILD_DATE " " BUILD_TIME "\n");
    t_writestring("The GCC/C++ sibling of Medli\n");
    t_writestring(COPYRIGHT "\n");
    t_writestring("Released under the BSD-3 Clause Clear license\n");
    t_writestring("\n");
    t_writestring("Type 'help' for a list of commands.\n");
    t_writestring("Welcome back, " SHELL_USERNAME "!\n\n");

    /* Initialise the VFS: probe for CD-ROM, set CWD to "/". */
    vfs_init();
    /* Auto-mount the appropriate filesystem based on the boot device. */
    vfs_auto_mount();

    while (1) {
        shell_print_prompt();
        shell_readline(buf, SHELL_MAX_INPUT);

        /* Save the raw input line before shell_parse() tokenises it in-place. */
        history_push(buf);

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
        /* ISO9660 commands */
        case CMD_ISOLS:      cmd_isols(argc, argv);       break;
        /* Installer */
        case CMD_INSTALL:    cmd_install();               break;
        case CMD_VICS:       cmd_vics(argc, argv);        break;
        case CMD_EJECT:      cmd_eject(argc, argv);       break;
        case CMD_RING3TEST:  cmd_ring3test();             break;
        case CMD_PANIC:      cmd_panic(argc, argv);       break;
        case CMD_CHAINLOAD:  cmd_chainload(argc, argv);   break;
        case CMD_EXEC:       cmd_exec(argc, argv);         break;
        /* File I/O */
        case CMD_WRITE:      cmd_write(argc, argv);        break;
        case CMD_TOUCH:      cmd_touch(argc, argv);        break;
        case CMD_CP:         cmd_cp(argc, argv);           break;
        default:
            /* Medli-style error: red text, matching CommandConsole.cs */
            t_setcolor(SHELL_ERROR_COLOR_VGA);
            t_writestring("The command '");
            t_writestring(argv[0]);
            t_writestring("' is not supported. Please type help for more information.\n\n");
            t_setcolor(SHELL_COLOR_VGA);
            break;
        }
    }
}

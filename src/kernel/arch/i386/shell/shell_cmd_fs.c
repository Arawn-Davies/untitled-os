/*
 * shell_cmd_fs.c -- filesystem shell builtins.
 *
 * Builtins live in the shell because they mutate parent state (cd) or are
 * cheap kernel-side ops with no userland equivalent yet.  ls/cat/cp/mv/rm
 * are external -- they ship as applets in fsutil.elf and are reached via
 * the shell's PATH-lookup -> fsutil-fallback chain in shell.c.
 */

#include "shell_priv.h"

#include <kernel/tty.h>
#include <kernel/fat32.h>
#include <kernel/vfs.h>
#include <kernel/iso9660.h>
#include <kernel/partition.h>
#include <kernel/ide.h>

static disk_parts_t s_cmd_parts;

static void cmd_mount(int argc, char **argv)
{
    if (argc < 3) {
        t_writestring("Usage: mount <drive> <part>\n");
        return;
    }

    uint8_t drive    = (uint8_t)parse_uint(argv[1]);
    int     part_num = (int)parse_uint(argv[2]);

    int err = part_probe(drive, &s_cmd_parts);
    if (err) {
        t_writestring("mount: drive not accessible\n");
        return;
    }

    int part_idx = part_num - 1;
    if (part_idx < 0 || part_idx >= s_cmd_parts.count) {
        t_writestring("mount: invalid partition number");
        if (s_cmd_parts.count > 0) {
            t_writestring(" (valid: 1-");
            t_dec((uint32_t)s_cmd_parts.count);
            t_putchar(')');
        }
        t_writestring("\n       (use lspart ");
        t_dec(drive);
        t_writestring(" to list partitions)\n");
        return;
    }

    uint32_t lba = s_cmd_parts.parts[part_idx].lba_start;

    err = fat32_mount(drive, lba);
    if (err) {
        t_writestring("mount: not a valid FAT32 volume (error ");
        t_dec((uint32_t)(-err));
        t_writestring(")\n");
        return;
    }

    vfs_notify_hd_mounted();

    t_writestring("Mounted FAT32  drive ");
    t_dec(drive);
    t_writestring("  partition ");
    t_dec((uint32_t)part_num);
    t_writestring("  LBA ");
    t_dec(lba);
    t_writestring("\ncwd: ");
    t_writestring(vfs_getcwd());
    t_putchar('\n');
}

static void cmd_umount(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (!fat32_mounted()) {
        t_writestring("umount: no volume mounted\n");
        return;
    }
    fat32_unmount();
    vfs_notify_hd_unmounted();
    t_writestring("Volume unmounted.\n");
}

static void cmd_cd(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring(vfs_getcwd());
        t_putchar('\n');
        return;
    }
    if (vfs_cd(argv[1]) == 0) {
        t_writestring(vfs_getcwd());
        t_putchar('\n');
    }
}

static void cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring("Usage: mkdir <path>...\n");
        return;
    }
    for (int i = 1; i < argc; i++) {
        int err = vfs_mkdir(argv[i]);
        switch (err) {
        case  0: break;
        case -1: t_writestring("mkdir: path error: ");    t_writestring(argv[i]); t_putchar('\n'); break;
        case -2: t_writestring("mkdir: I/O error: ");     t_writestring(argv[i]); t_putchar('\n'); break;
        case -4: t_writestring("mkdir: disk full: ");     t_writestring(argv[i]); t_putchar('\n'); break;
        case -6: t_writestring("mkdir: already exists: ");t_writestring(argv[i]); t_putchar('\n'); break;
        default:
            if (err < 0) {
                t_writestring("mkdir: error ");
                t_dec((uint32_t)(-err));
                t_writestring(": ");
                t_writestring(argv[i]);
                t_putchar('\n');
            }
            break;
        }
    }
}

static void cmd_mkfs(int argc, char **argv)
{
    if (argc < 3) {
        t_writestring("Usage: mkfs <drive> <part>\n");
        return;
    }

    uint8_t drive    = (uint8_t)parse_uint(argv[1]);
    int     part_num = (int)parse_uint(argv[2]);

    int err = part_probe(drive, &s_cmd_parts);
    if (err) {
        t_writestring("mkfs: drive not accessible\n");
        return;
    }

    int part_idx = part_num - 1;
    if (part_idx < 0 || part_idx >= s_cmd_parts.count) {
        t_writestring("mkfs: invalid partition number");
        if (s_cmd_parts.count > 0) {
            t_writestring(" (valid: 1-");
            t_dec((uint32_t)s_cmd_parts.count);
            t_putchar(')');
        }
        t_writestring("\n      (use lspart ");
        t_dec(drive);
        t_writestring(" to list partitions)\n");
        return;
    }

    uint32_t lba     = s_cmd_parts.parts[part_idx].lba_start;
    uint32_t sectors = s_cmd_parts.parts[part_idx].lba_count;

    t_writestring("Formatting drive ");
    t_dec(drive);
    t_writestring(" partition ");
    t_dec((uint32_t)part_num);
    t_writestring(" (");
    t_dec(sectors / 2048u);
    t_writestring(" MiB) as FAT32...\n");

    err = fat32_mkfs(drive, lba, sectors);
    if (err == -6) {
        t_writestring("mkfs: partition too small for FAT32 (need >= 32 MiB)\n");
        return;
    }
    if (err) {
        t_writestring("mkfs: I/O error (");
        t_dec((uint32_t)(-err));
        t_writestring(")\n");
        return;
    }

    t_writestring("Done.  Mount with: mount ");
    t_dec(drive);
    t_putchar(' ');
    t_dec((uint32_t)part_num);
    t_putchar('\n');
}

static void cmd_isols(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring("Usage: isols <drive> [path]\n");
        return;
    }

    uint8_t drive = (uint8_t)parse_uint(argv[1]);
    const char *path = (argc >= 3) ? argv[2] : "/";

    int err = iso9660_ls(drive, path);
    if (err == -2)
        t_writestring("isols: path not found or not ISO9660\n");
    else if (err)
        t_writestring("isols: I/O error\n");
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 3) {
        t_writestring("Usage: write <file> <text...>\n");
        return;
    }

    static char s_write_buf[SHELL_MAX_INPUT];
    size_t off = 0;

    for (int i = 2; i < argc; i++) {
        if (i > 2 && off < sizeof(s_write_buf) - 1)
            s_write_buf[off++] = ' ';
        const char *s = argv[i];
        while (*s && off < sizeof(s_write_buf) - 1)
            s_write_buf[off++] = *s++;
    }
    if (off < sizeof(s_write_buf) - 1)
        s_write_buf[off++] = '\n';
    s_write_buf[off] = '\0';

    static char s_write_path[VFS_PATH_MAX];
    const char *arg = argv[1];
    const char *cwd = vfs_getcwd();
    if (arg[0] == '/') {
        strncpy(s_write_path, arg, VFS_PATH_MAX - 1);
        s_write_path[VFS_PATH_MAX - 1] = '\0';
    } else {
        size_t cl = strlen(cwd), al = strlen(arg);
        if (cl + 1 + al >= VFS_PATH_MAX) { t_writestring("write: path too long\n"); return; }
        size_t p = 0;
        memcpy(s_write_path, cwd, cl); p += cl;
        if (cwd[cl - 1] != '/') s_write_path[p++] = '/';
        memcpy(s_write_path + p, arg, al + 1);
    }

    int err = vfs_write_file(s_write_path, s_write_buf, (uint32_t)off);
    if (err) {
        t_writestring("write: error ");
        t_dec((uint32_t)(-err));
        t_putchar('\n');
    }
}

static void cmd_touch(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring("Usage: touch <file>...\n");
        return;
    }

    static char s_touch_path[VFS_PATH_MAX];
    const char *cwd = vfs_getcwd();

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg[0] == '/') {
            strncpy(s_touch_path, arg, VFS_PATH_MAX - 1);
            s_touch_path[VFS_PATH_MAX - 1] = '\0';
        } else {
            size_t cl = strlen(cwd), al = strlen(arg);
            if (cl + 1 + al >= VFS_PATH_MAX) {
                t_writestring("touch: path too long: ");
                t_writestring(arg);
                t_putchar('\n');
                continue;
            }
            size_t p = 0;
            memcpy(s_touch_path, cwd, cl); p += cl;
            if (cwd[cl - 1] != '/') s_touch_path[p++] = '/';
            memcpy(s_touch_path + p, arg, al + 1);
        }

        int err = vfs_write_file(s_touch_path, "", 0);
        if (err) {
            t_writestring("touch: error ");
            t_dec((uint32_t)(-err));
            t_writestring(": ");
            t_writestring(arg);
            t_putchar('\n');
        }
    }
}

const shell_cmd_entry_t fs_cmds[] = {
    { "mount",  cmd_mount  },
    { "umount", cmd_umount },
    { "cd",     cmd_cd     },
    { "mkdir",  cmd_mkdir  },
    { "mkfs",   cmd_mkfs   },
    { "isols",  cmd_isols  },
    { "write",  cmd_write  },
    { "touch",  cmd_touch  },
    { NULL, NULL }
};

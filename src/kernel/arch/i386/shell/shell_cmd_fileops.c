/*
 * shell_cmd_fileops.c -- file operation shell commands (delete, move, rename).
 *
 * Commands: rm  rmdir  mv
 *
 * TODO: Implement FAT32 delete and rename APIs first.
 * See SURVEY.md for details on what's missing.
 */

#include "shell_priv.h"

#include <kernel/tty.h>
#include <kernel/vfs.h>

/* ---------------------------------------------------------------------------
 * rm – delete a file
 *
 * TODO: Requires vfs_delete_file(path) implementation
 * TODO: Error handling for common cases (not found, is directory, etc.)
 * --------------------------------------------------------------------------- */
static void cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring("Usage: rm <file>\n");
        return;
    }

    /* TODO: Implement file deletion
     * - Verify it's a file, not a directory
     * - Call vfs_delete_file(argv[1])
     * - Report success or error
     */
    t_writestring("rm: not yet implemented\n");
}

/* ---------------------------------------------------------------------------
 * rmdir – delete an empty directory
 *
 * TODO: Requires vfs_delete_dir(path) implementation
 * TODO: Error handling (not empty, not found, not a directory)
 * --------------------------------------------------------------------------- */
static void cmd_rmdir(int argc, char **argv)
{
    if (argc < 2) {
        t_writestring("Usage: rmdir <directory>\n");
        return;
    }

    /* TODO: Implement directory deletion
     * - Verify it's a directory
     * - Verify it's empty
     * - Call vfs_delete_dir(argv[1])
     * - Report success or error
     */
    t_writestring("rmdir: not yet implemented\n");
}

/* ---------------------------------------------------------------------------
 * mv – move/rename a file or directory
 *
 * TODO: Requires vfs_rename(old_path, new_path) implementation
 * TODO: Error handling (source not found, destination exists, etc.)
 * --------------------------------------------------------------------------- */
static void cmd_mv(int argc, char **argv)
{
    if (argc < 3) {
        t_writestring("Usage: mv <src> <dst>\n");
        return;
    }

    /* TODO: Implement file/directory move
     * - Verify source exists (file or directory)
     * - Call vfs_rename(argv[1], argv[2])
     * - Report success or error
     */
    t_writestring("mv: not yet implemented\n");
}

/* ---------------------------------------------------------------------------
 * Module table
 * --------------------------------------------------------------------------- */

const shell_cmd_entry_t fileops_cmds[] = {
    { "rm",    cmd_rm    },
    { "rmdir", cmd_rmdir },
    { "mv",    cmd_mv    },
    { NULL, NULL }
};

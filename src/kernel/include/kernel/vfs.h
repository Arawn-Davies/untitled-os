#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

/*
 * vfs.h — lightweight Virtual Filesystem layer.
 *
 * Provides a single, unified path namespace:
 *
 *   /          – virtual root; ls shows available mount-points
 *   /hd/…      – FAT32 hard-disk partition (mounted via cmd_mount)
 *   /cdrom/…   – ISO9660 CD-ROM (auto-detected on vfs_init)
 *
 * All shell commands (ls, cd, cat, mkdir) use this layer so they work
 * transparently across both filesystems.
 */

#include <kernel/types.h>

/* Maximum length of any VFS path string (including NUL terminator). */
#define VFS_PATH_MAX  256

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */

/*
 * vfs_init – probe IDE bus for an ISO9660 CD-ROM and reset the CWD to "/".
 * Must be called after ide_init().
 */
void vfs_init(void);

/* -------------------------------------------------------------------------
 * State notifications (called by mount/umount commands)
 * ---------------------------------------------------------------------- */

/*
 * vfs_notify_hd_mounted   – FAT32 volume has just been mounted.
 *                           Moves the CWD to "/hd" when currently at "/".
 * vfs_notify_hd_unmounted – FAT32 volume has just been unmounted.
 *                           Resets the CWD to "/" when it was under "/hd".
 */
void vfs_notify_hd_mounted(void);
void vfs_notify_hd_unmounted(void);

/* -------------------------------------------------------------------------
 * Current working directory
 * ---------------------------------------------------------------------- */

/* Return a pointer to the current VFS path (e.g. "/hd/boot/grub"). */
const char *vfs_getcwd(void);

/* -------------------------------------------------------------------------
 * Filesystem operations
 * ---------------------------------------------------------------------- */

/*
 * vfs_ls    – list directory contents.  NULL → use CWD.
 * vfs_cd    – change the current VFS working directory.
 * vfs_cat   – read and print a file's contents to the terminal.
 * vfs_mkdir – create a directory (FAT32 only).
 *
 * All functions return 0 on success, negative on error.
 */
int vfs_ls(const char *path);
int vfs_cd(const char *path);
int vfs_cat(const char *path);
int vfs_mkdir(const char *path);

/*
 * vfs_read_file  – read a file into a caller-supplied buffer.
 * vfs_write_file – create or overwrite a file (FAT32 only).
 *
 * Return 0 on success, negative on error.
 */
int vfs_read_file(const char *path, void *buf, uint32_t bufsz, uint32_t *out_sz);
int vfs_write_file(const char *path, const void *buf, uint32_t size);

#endif /* _KERNEL_VFS_H */

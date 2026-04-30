"""HDD mount verification group.

Verifies that vfs_auto_mount() successfully mounted the FAT32 partition
from the boot HDD at /hd.

The shell calls vfs_init() and vfs_auto_mount() near the top of shell_run(),
then enters its read loop which calls keyboard_getchar().  This group
continues execution until keyboard_getchar() is first entered — guaranteeing
that vfs_auto_mount() has completed — then checks fat32_mounted().

A pass confirms the full HDD boot path:
  grub-install core.img → Multiboot2 → IDE detected ATA drive →
  part_probe found MBR FAT32 partition → fat32_mount() accepted the BPB.
"""

import gdb

NAME = 'HDD Mount'


def run():
    # Continue until the shell blocks in keyboard_getchar(), which is called
    # after vfs_auto_mount() completes.  This avoids a race where
    # timer_callback (the previous stopping point) fires before vfs_auto_mount
    # finishes, causing fat32_mounted() to return 0 spuriously.
    kbp = gdb.Breakpoint('keyboard_getchar', internal=True, temporary=True)
    kbp.silent = True
    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('FAIL: GDB error waiting for keyboard_getchar: ' + str(exc),
              flush=True)
        return False

    try:
        mounted = int(gdb.parse_and_eval('fat32_mounted()'))
    except gdb.error as exc:
        print('FAIL: could not evaluate fat32_mounted(): ' + str(exc),
              flush=True)
        return False

    if not mounted:
        print('FAIL: fat32_mounted() returned 0 — /hd was NOT mounted',
              flush=True)
        return False

    print('PASS: fat32_mounted() returned {} — /hd is mounted'.format(mounted),
          flush=True)
    print('GROUP PASS: ' + NAME, flush=True)
    return True

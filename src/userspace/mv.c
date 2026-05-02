/*
 * mv.c -- userspace file/directory move/rename utility.
 */

#include "syscall.h"

static void write_str(const char *s)
{
    unsigned int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        write_str("Usage: mv <src> <dst>\n");
        sys_exit(1);
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    if (sys_rename_file(src, dst) != 0) {
        write_str("mv: failed to rename '");
        write_str(src);
        write_str("' to '");
        write_str(dst);
        write_str("'\n");
        sys_exit(1);
    }

    sys_exit(0);
    return 0;
}

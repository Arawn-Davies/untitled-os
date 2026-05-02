/*
 * mv.c -- userspace file move/rename utility.
 *
 * TODO: Implement file move/rename via sys_rename_file() syscall.
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

    /* TODO: Implement file move/rename
     * - Call sys_rename_file(argv[1], argv[2])
     * - Report success or error
     */
    write_str("mv: not yet implemented\n");
    sys_exit(1);
    return 0;
}

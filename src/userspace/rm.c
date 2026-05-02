/*
 * rm.c -- userspace file deletion utility.
 *
 * TODO: Implement file deletion via sys_delete_file() syscall.
 * TODO: Add support for -r (recursive directory deletion).
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
    if (argc < 2) {
        write_str("Usage: rm <file>\n");
        sys_exit(1);
    }

    /* TODO: Implement file deletion
     * - Call sys_delete_file(argv[1])
     * - Report success or error
     * - Handle -r flag for recursive deletion
     */
    write_str("rm: not yet implemented\n");
    sys_exit(1);
    return 0;
}

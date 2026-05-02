/*
 * rm.c -- userspace file/directory deletion utility.
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
    int dir_mode = 0;
    int argi = 1;

    if (argc >= 2 && argv[1][0] == '-') {
        if (argv[1][1] == 'd' && argv[1][2] == '\0') {
            dir_mode = 1;
            argi = 2;
        }
    }

    if (argi >= argc) {
        write_str("Usage: rm [-d] <path>\n");
        sys_exit(1);
    }

    const char *path = argv[argi];
    int ret;

    if (dir_mode)
        ret = sys_delete_dir(path);
    else
        ret = sys_delete_file(path);

    if (ret != 0) {
        write_str("rm: failed to delete '");
        write_str(path);
        write_str("'\n");
        sys_exit(1);
    }

    sys_exit(0);
    return 0;
}

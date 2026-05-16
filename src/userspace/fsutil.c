/*
 * fsutil.c -- multicall filesystem utility (busybox-style dispatcher).
 *
 * Usage:
 *   fsutil <ls|cat|pwd|cp|mv|rm> [args...]
 *
 * The kernel shell forwards commands like `ls`, `cp`, `mv`, `rm`, `cat`
 * to this single binary by passing the original command as argv[1].
 */

#include "syscall.h"

static unsigned int str_len(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static int streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static void write_fd(int fd, const char *s)
{
    sys_write(fd, s, str_len(s));
}

static int cmd_ls(int argc, char **argv)
{
    static char buf[4096];

    if (argc < 2) {
        int n = sys_ls_dir(".", buf, (unsigned int)sizeof(buf));
        if (n < 0) {
            write_fd(1, "ls: cannot list directory\n");
            return 1;
        }
        if (n == 0) write_fd(1, "(empty)\n");
        else        sys_write(1, buf, (unsigned int)n);
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (argc > 2) {
            if (i > 1) write_fd(1, "\n");
            write_fd(1, argv[i]);
            write_fd(1, ":\n");
        }

        int n = sys_ls_dir(argv[i], buf, (unsigned int)sizeof(buf));
        if (n < 0) {
            write_fd(1, "ls: cannot list directory\n");
            return 1;
        }
        if (n == 0) write_fd(1, "(empty)\n");
        else        sys_write(1, buf, (unsigned int)n);
    }
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    static unsigned char buf[4096];

    if (argc < 2) {
        write_fd(1, "Usage: cat <file>...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        int fd = sys_open(argv[i], O_RDONLY);
        if (fd < 0) {
            write_fd(1, "cat: cannot open '");
            write_fd(1, argv[i]);
            write_fd(1, "'\n");
            return 1;
        }

        while (1) {
            long n = sys_read(fd, buf, (unsigned int)sizeof(buf));
            if (n < 0) {
                write_fd(1, "cat: read error on '");
                write_fd(1, argv[i]);
                write_fd(1, "'\n");
                sys_close(fd);
                return 1;
            }
            if (n == 0) break;
            sys_write(1, buf, (unsigned int)n);
        }
        sys_close(fd);
    }

    return 0;
}

static int cmd_pwd(int argc, char **argv)
{
    if (argc >= 2 && argv[1] && argv[1][0]) {
        write_fd(1, argv[1]);
        write_fd(1, "\n");
        return 0;
    }
    write_fd(1, "/\n");
    return 0;
}

/* 64 KiB static buffer - keep in sync with kernel syscall file max. */
#define FSUTIL_CP_BUF_SIZE 65536u
static unsigned char s_cp_buf[FSUTIL_CP_BUF_SIZE];
/* Single-byte overflow probe used to detect sources larger than 64 KiB. */
static unsigned char s_cp_overflow_byte;

static int cmd_cp(int argc, char **argv)
{
    if (argc < 3) {
        write_fd(1, "Usage: cp <src> <dst>\n");
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    int fd = sys_open(src, O_RDONLY);
    if (fd < 0) {
        write_fd(1, "cp: cannot open '");
        write_fd(1, src);
        write_fd(1, "'\n");
        return 1;
    }

    long n = sys_read(fd, s_cp_buf, (unsigned int)sizeof(s_cp_buf));
    if (n < 0) {
        write_fd(1, "cp: read error on '");
        write_fd(1, src);
        write_fd(1, "'\n");
        sys_close(fd);
        return 1;
    }

    if ((unsigned long)n == (unsigned long)sizeof(s_cp_buf)) {
        long more = sys_read(fd, &s_cp_overflow_byte, 1u);
        if (more > 0) {
            write_fd(1, "cp: source too large (max 64 KiB)\n");
            sys_close(fd);
            return 1;
        }
    }
    sys_close(fd);

    if (sys_write_file(dst, s_cp_buf, (unsigned int)n) != 0) {
        write_fd(1, "cp: cannot write '");
        write_fd(1, dst);
        write_fd(1, "'\n");
        return 1;
    }

    return 0;
}

static int cmd_mv(int argc, char **argv)
{
    if (argc < 3) {
        write_fd(1, "Usage: mv <src> <dst>\n");
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    if (sys_rename_file(src, dst) != 0) {
        write_fd(1, "mv: failed to rename '");
        write_fd(1, src);
        write_fd(1, "' to '");
        write_fd(1, dst);
        write_fd(1, "'\n");
        return 1;
    }

    return 0;
}

static int cmd_rm(int argc, char **argv)
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
        write_fd(1, "Usage: rm [-d] <path>...\n");
        return 1;
    }

    int rc = 0;
    for (int i = argi; i < argc; i++) {
        int ret = dir_mode ? sys_delete_dir(argv[i]) : sys_delete_file(argv[i]);
        if (ret != 0) {
            write_fd(1, "rm: failed to delete '");
            write_fd(1, argv[i]);
            write_fd(1, "'\n");
            rc = 1;
        }
    }
    return rc;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        write_fd(1, "Usage: fsutil <ls|cat|pwd|cp|mv|rm> [args...]\n");
        sys_exit(1);
    }

    const char *cmd = argv[1];
    int sub_argc = argc - 1;
    char **sub_argv = argv + 1;

    int rc = 1;
    if (streq(cmd, "ls"))      rc = cmd_ls(sub_argc, sub_argv);
    else if (streq(cmd, "cat")) rc = cmd_cat(sub_argc, sub_argv);
    else if (streq(cmd, "pwd")) rc = cmd_pwd(sub_argc, sub_argv);
    else if (streq(cmd, "cp"))  rc = cmd_cp(sub_argc, sub_argv);
    else if (streq(cmd, "mv"))  rc = cmd_mv(sub_argc, sub_argv);
    else if (streq(cmd, "rm"))  rc = cmd_rm(sub_argc, sub_argv);
    else {
        write_fd(1, "fsutil: unknown command '");
        write_fd(1, cmd);
        write_fd(1, "'\n");
        write_fd(1, "Usage: fsutil <ls|cat|pwd|cp|mv|rm> [args...]\n");
    }

    sys_exit(rc);
    return rc;
}

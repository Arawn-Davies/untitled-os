#include "syscall.h"

static char buf[4096];

static unsigned int ustrlen(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : ".";
    int n = sys_ls_dir(path, buf, (unsigned int)sizeof(buf));
    if (n < 0) {
        const char *e = "ls: cannot list directory\n";
        sys_write(1, e, ustrlen(e));
        sys_exit(1);
    }
    if (n == 0) {
        const char *e = "(empty)\n";
        sys_write(1, e, ustrlen(e));
    } else {
        sys_write(1, buf, (unsigned int)n);
    }
    return 0;
}

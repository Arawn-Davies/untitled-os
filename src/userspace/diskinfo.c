#include "syscall.h"

static char buf[1024];

static unsigned int ustrlen(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    int n = sys_disk_info(buf, (unsigned int)sizeof(buf));
    if (n <= 0) {
        const char *msg = "No drives detected.\n";
        sys_write(1, msg, ustrlen(msg));
        sys_exit(1);
    }
    sys_write(1, buf, (unsigned int)n);
    return 0;
}

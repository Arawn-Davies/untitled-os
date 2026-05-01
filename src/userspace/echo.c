#include "syscall.h"

static void write_str(const char *s)
{
    unsigned int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            sys_write(1, " ", 1);
        write_str(argv[i]);
    }
    sys_write(1, "\n", 1);
    return 0;
}

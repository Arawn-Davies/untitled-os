#include "syscall.h"

static void write_str(const char *s)
{
    unsigned int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

int main(void)
{
    char name[64];

    write_str("What is your name? ");

    long n = sys_read(0, name, sizeof(name) - 1);
    if (n > 0) {
        /* Strip trailing newline from shell_readline. */
        if (name[n - 1] == '\n')
            n--;
        name[n] = '\0';
    } else {
        name[0] = '\0';
    }

    write_str("Hello, ");
    write_str(name[0] ? name : "stranger");
    write_str("!\n");

    return 0;
}

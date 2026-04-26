#include "syscall.h"

static void write_str(const char *s)
{
    unsigned int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

int main(void)
{
    char buf[128];

    write_str("Hello from ring-3 userspace!\n");
    write_str("Type something: ");

    long n = sys_read(0, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        write_str("You typed: ");
        sys_write(1, buf, (unsigned int)n);
    }

    write_str("Goodbye.\n");
    return 0;
}

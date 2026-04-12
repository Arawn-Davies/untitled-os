/*
 * echo.c -- print arguments to the terminal.
 *
 * Usage: echo [string ...]
 *
 * Writes each argument joined by a single space, followed by a newline,
 * using the SYS_WRITE syscall.  Mirrors the behaviour of the kernel shell's
 * built-in `echo` command so that the same output is produced once this
 * utility is loaded and executed by the ELF loader.
 */

#include <sys.h>

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            sys_write(" ");
        sys_write(argv[i]);
    }
    sys_write("\n");
    return 0;
}

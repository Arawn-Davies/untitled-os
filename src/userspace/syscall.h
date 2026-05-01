#ifndef _USERSPACE_SYSCALL_H
#define _USERSPACE_SYSCALL_H

/* Syscall numbers — Linux i386 ABI subset. */
#define SYS_EXIT    1
#define SYS_READ    3
#define SYS_WRITE   4
#define SYS_OPEN    5
#define SYS_CLOSE   6
#define SYS_BRK     45
#define SYS_YIELD   158
#define SYS_DEBUG   100  /* Makar extension */

/* open() flags (only O_RDONLY supported currently). */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

/* Raw syscall stubs. */
static inline long syscall1(long nr, long a1)
{
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(nr), "b"(a1) : "memory");
    return ret;
}

static inline long syscall2(long nr, long a1, long a2)
{
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(nr), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline long syscall3(long nr, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "0"(nr), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

/* POSIX-compatible wrappers. */

static inline void sys_exit(int status)
{
    syscall1(SYS_EXIT, (long)status);
    __builtin_unreachable();
}

static inline long sys_read(int fd, void *buf, unsigned int len)
{
    return syscall3(SYS_READ, (long)fd, (long)buf, (long)len);
}

static inline long sys_write(int fd, const void *buf, unsigned int len)
{
    return syscall3(SYS_WRITE, (long)fd, (long)buf, (long)len);
}

static inline int sys_open(const char *path, int flags)
{
    return (int)syscall2(SYS_OPEN, (long)path, (long)flags);
}

static inline int sys_close(int fd)
{
    return (int)syscall1(SYS_CLOSE, (long)fd);
}

static inline long sys_brk(void *addr)
{
    return syscall1(SYS_BRK, (long)addr);
}

static inline void sys_yield(void)
{
    syscall1(SYS_YIELD, 0);
}

#endif

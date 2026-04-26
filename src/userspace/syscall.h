#ifndef _USERSPACE_SYSCALL_H
#define _USERSPACE_SYSCALL_H

#define SYS_EXIT    1
#define SYS_READ    3
#define SYS_WRITE   4
#define SYS_OPEN    5
#define SYS_CLOSE   6
#define SYS_BRK     45
#define SYS_YIELD   158
#define SYS_DEBUG   100

static inline long syscall1(long nr, long a1)
{
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "0"(nr), "b"(a1)
        : "memory");
    return ret;
}

static inline long syscall3(long nr, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "0"(nr), "b"(a1), "c"(a2), "d"(a3)
        : "memory");
    return ret;
}

static inline void sys_exit(int status)
{
    syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

static inline long sys_write(int fd, const void *buf, unsigned int len)
{
    return syscall3(SYS_WRITE, fd, (long)buf, (long)len);
}

static inline long sys_read(int fd, void *buf, unsigned int len)
{
    return syscall3(SYS_READ, fd, (long)buf, (long)len);
}

#endif

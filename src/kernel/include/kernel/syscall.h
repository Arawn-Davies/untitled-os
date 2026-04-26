#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

#include <stdint.h>
#include <kernel/isr.h>

/*
 * Syscall numbers — Linux i386 ABI subset.
 *
 * Registers (int 0x80 calling convention):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3
 *   Return value written back to EAX (negative errno on error).
 */
#define SYS_EXIT    1    /* void exit(int status)                         */
#define SYS_READ    3    /* ssize_t read(int fd, void *buf, size_t len)   */
#define SYS_WRITE   4    /* ssize_t write(int fd, const void *buf, size_t)*/
#define SYS_OPEN    5    /* int open(const char *path, int flags)         */
#define SYS_CLOSE   6    /* int close(int fd)                             */
#define SYS_BRK     45   /* void *brk(void *addr)                         */
#define SYS_YIELD   158  /* void sched_yield(void)                        */
#define SYS_DEBUG   100  /* void debug(uint32_t checkpoint)  [Makar ext]  */

/* open() flags */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

/* Well-known file descriptors */
#define FD_STDIN    0
#define FD_STDOUT   1
#define FD_STDERR   2

/* Maximum size of a file that can be opened via SYS_OPEN (64 KiB). */
#define SYSCALL_FILE_MAX  (64u * 1024u)

/*
 * g_ring3_last_cp — last SYS_DEBUG checkpoint value received from ring-3.
 * Reset to 0 before launching a ring-3 test task; read after it exits.
 */
extern volatile uint32_t g_ring3_last_cp;

/*
 * syscall_init — register int 0x80 in the interrupt-handler table.
 *
 * Must be called after init_descriptor_tables() and tasking_init().
 */
void syscall_init(void);

/*
 * syscall_dispatch — the int 0x80 C handler; exposed for in-kernel testing.
 */
void syscall_dispatch(registers_t *regs);

/*
 * syscall_reset_fds — close all open file descriptors.
 *
 * Called by elf_exec() before launching a new process so that a fresh exec
 * does not inherit stale file handles from the previous run.
 */
void syscall_reset_fds(void);

#endif /* _KERNEL_SYSCALL_H */

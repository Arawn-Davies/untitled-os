/*
 * sys.h -- Makar user-space syscall interface.
 *
 * Thin inline wrappers around int 0x80, following the Linux i386 ABI
 * calling convention used by the Makar kernel:
 *
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3, ESI = arg4, EDI = arg5
 *   return value in EAX
 *
 * Syscall numbers must stay in sync with src/kernel/include/kernel/syscall.h.
 */

#pragma once

/* ----------------------------------------------------------------------- */
/* Syscall numbers                                                          */
/* ----------------------------------------------------------------------- */

#define SYS_EXIT   1     /* terminate current task                          */
#define SYS_WRITE  4     /* write NUL-terminated string at EBX to terminal  */
#define SYS_YIELD  158   /* voluntarily yield the CPU to another task        */

/* ----------------------------------------------------------------------- */
/* Inline syscall wrappers                                                  */
/* ----------------------------------------------------------------------- */

/*
 * sys_write -- write a NUL-terminated string to the kernel terminal.
 *
 * The kernel writes the string via t_writestring(), so output appears on
 * both the VESA/VGA display and the serial console.
 */
static inline void sys_write(const char *s)
{
    __asm__ volatile("int $0x80"
                     :
                     : "a"(SYS_WRITE), "b"(s)
                     : "memory");
}

/*
 * sys_exit -- terminate the current task.
 *
 * The exit code is passed in EBX for future use; the current kernel
 * implementation does not yet propagate it to a parent process.
 */
__attribute__((noreturn))
static inline void sys_exit(int code)
{
    __asm__ volatile("int $0x80"
                     :
                     : "a"(SYS_EXIT), "b"(code)
                     : "memory");
    __builtin_unreachable();
}

/*
 * sys_yield -- voluntarily give up the CPU.
 *
 * Useful for cooperative multitasking; the kernel round-robin scheduler
 * picks the next READY task.
 */
static inline void sys_yield(void)
{
    __asm__ volatile("int $0x80"
                     :
                     : "a"(SYS_YIELD)
                     : "memory");
}

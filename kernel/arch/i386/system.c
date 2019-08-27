#include <kernel/system.h>

inline void io_wait()
{
    /* TODO: This is probably fragile. */
    asm volatile ( "jmp 1f\n\t"
                   "1:jmp 2f\n\t"
                   "2:" );
}

//Halts the CPU, executes when there's an unrecoverable exception or some other error
inline void halt()
{
    asm volatile ("hlt");
}

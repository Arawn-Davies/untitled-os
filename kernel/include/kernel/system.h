#ifndef _KERNEL_SYSTEM_H_
#define _KERNEL_SYSTEM_H_

#include <kernel/types.h>
#include <stdio.h>

// Waits for 400ns, used for reading from ATA/ATAPI devices with IRQs etc.
inline void io_wait();

// Halts the CPU, executes when there's an unrecoverable exception or some other error
inline void halt();



#define ASSERT(b) ((b) ? (void)0:panic_assert(__FILE__, __LINE__, #b));
void panic(char* msg, char *file, uint32_t line);
void panic_assert(char *file, uint32_t line, char *desc);
#define PANIC(msg) panic(msg, __FILE__, __LINE__);

#endif

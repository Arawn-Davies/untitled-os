#ifndef _KERNEL_SYSTEM_H_
#define _KERNEL_SYSTEM_H_

// Waits for 400ns, used for reading from ATA/ATAPI devices with IRQs etc.
inline void io_wait();

// Halts the CPU, executes when there's an unrecoverable exception or some other error
inline void halt();

#endif

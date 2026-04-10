#ifndef _KERNEL_DEBUG_H
#define _KERNEL_DEBUG_H

// Register GDB-aware handlers for INT 1 (debug exception) and INT 3 (breakpoint).
// These print register state to the terminal and serial port instead of panicking,
// allowing QEMU's GDB stub to remain in control during a debugging session.
void init_debug_handlers();

#endif // _KERNEL_DEBUG_H

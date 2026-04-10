#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <stdint.h>

// Initialise the PS/2 keyboard driver and register the IRQ1 handler.
void keyboard_init(void);

// Block until a character is available, then return it.
char keyboard_getchar(void);

// Return the next character from the ring buffer, or 0 if the buffer is empty.
char keyboard_poll(void);

#endif // _KERNEL_KEYBOARD_H

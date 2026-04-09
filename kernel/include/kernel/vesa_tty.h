#ifndef _KERNEL_VESA_TTY_H
#define _KERNEL_VESA_TTY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Initialise the VESA text renderer.  Must be called after a successful
 * vesa_init().  Clears the framebuffer and sets default colours (white on
 * black).  Returns false if no framebuffer is available.
 */
bool vesa_tty_init(void);

/* Returns true once vesa_tty_init() has succeeded. */
bool vesa_tty_is_ready(void);

/*
 * Render a single character to the framebuffer at the current cursor
 * position, advancing the cursor.  Handles '\n', '\r', and '\b'.
 * Safe to call before vesa_tty_init() — exits immediately if not ready.
 */
void vesa_tty_putchar(char c);

/* Set foreground and background colours as 24-bit RGB (0x00RRGGBB). */
void vesa_tty_setcolor(uint32_t fg_rgb, uint32_t bg_rgb);

#endif

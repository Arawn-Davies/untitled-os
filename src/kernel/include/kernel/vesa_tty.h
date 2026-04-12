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

/* Returns the current cursor row (in character cells). */
uint32_t vesa_tty_get_row(void);

/* Returns the total number of character columns on the VESA display. */
uint32_t vesa_tty_get_cols(void);

/*
 * Render a single character to the framebuffer at the current cursor
 * position, advancing the cursor.  Handles '\n', '\r', and '\b'.
 * Safe to call before vesa_tty_init() — exits immediately if not ready.
 */
void vesa_tty_putchar(char c);

/* Set foreground and background colours as 24-bit RGB (0x00RRGGBB). */
void vesa_tty_setcolor(uint32_t fg_rgb, uint32_t bg_rgb);

/*
 * Render character c at a specific cell (col, row) without moving the cursor.
 * Safe to call before vesa_tty_init() — exits immediately if not ready.
 */
void vesa_tty_put_at(char c, uint32_t col, uint32_t row);

/*
 * Animate a spinning cursor at the top-right corner of the VESA display.
 * Call with the current timer tick count (same as t_spinner_tick).
 * Safe to call before vesa_tty_init() — exits immediately if not ready.
 */
void vesa_tty_spinner_tick(uint32_t tick);

/*
 * vesa_tty_clear – reset the cursor to (0, 0) and fill the framebuffer with
 * the current background colour without changing the fg/bg colour settings.
 * Use this after vesa_tty_setcolor() to apply a colour-scheme change.
 */
void vesa_tty_clear(void);

/*
 * Change the font scale factor.  scale=2 gives large glyphs (~25 lines at
 * 1024×768), scale=1 gives small glyphs (~50 lines at 1024×768).
 * Clears the screen and resets the cursor.
 * Safe to call before vesa_tty_init() — exits immediately if not ready.
 */
void vesa_tty_set_scale(uint32_t scale);

#endif

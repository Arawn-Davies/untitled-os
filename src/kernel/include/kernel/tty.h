#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Current number of VGA text rows (25 or 50). */
extern size_t t_height;

/* Current cursor position (0-based). */
extern size_t t_row;
extern size_t t_column;

/*
 * Write character c with the given colour attribute directly into the VGA
 * buffer at cell (x, y) without moving the cursor.
 */
void t_putentryat(char c, uint8_t color, size_t x, size_t y);

void terminal_initialize(void);
void terminal_set_rows(size_t rows);
void t_setcolor(uint8_t color);
void t_backspace();
void t_putchar(char c);
void t_write(const char *data, size_t size);
void t_writestring(const char *data);

void t_hex(uint32_t num);
void t_dec(uint32_t num);

void t_spinner_tick(uint32_t tick);

#endif

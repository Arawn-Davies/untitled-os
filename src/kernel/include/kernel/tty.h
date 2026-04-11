#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Current number of VGA text rows (25 or 50). */
extern size_t t_height;

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

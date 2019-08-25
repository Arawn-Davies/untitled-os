#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/vga.h>

size_t t_line_fill[VGA_WIDTH];
size_t t_row;
size_t t_column;
uint8_t t_color;
uint16_t* t_buffer;

void t_putentryat(char c, uint8_t color, size_t x, size_t y);
void t_scroll();

void terminal_initialize(void) {
	t_row = 0;
	t_column = 0;
	t_color = make_color(COLOR_WHITE, COLOR_BLACK);
	t_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			const size_t index = y * VGA_WIDTH + x;
			t_buffer[index] = make_vgaentry(' ', t_color);
		}
	}
}

void t_backspace()
{
	if (t_column == 0)
	{
		if (t_row > 0)
		{
			t_row--;
		}
		t_column = t_line_fill[t_row];
	}
	else
	{
		t_column--;
	}
	
	t_putentryat(32, t_color, t_column, t_row);
	update_cursor(t_row, t_column);
}

void t_setcolor(uint8_t color)
{
	t_color = color; 
}

void t_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	t_buffer[index] = make_vgaentry(c, color);
}

void t_putchar(char c)
{
	if (c != '\n') 
	{
		t_putentryat(c, t_color, t_column, t_row);
	}

	if (++t_column == VGA_WIDTH || c == '\n')
	{	
		t_line_fill[t_row] = t_column - 1;
		t_column = 0;
		if (++t_row == VGA_HEIGHT)
		{
			t_scroll();
		}
	}

	update_cursor(t_row, t_column);
}

void t_scroll() 
{
	t_row--;
	for (size_t y = 0; y < VGA_HEIGHT - 1; y++) 
	{
		for (size_t x = 0; x < VGA_WIDTH; x++) 
		{
			const size_t src_index = y * VGA_WIDTH + x;
			const size_t dst_index = (y + 1) * VGA_WIDTH + x;
			t_buffer[src_index] = t_buffer[dst_index];
		}
    	t_line_fill[y] = t_line_fill[y + 1];
	}

	for (size_t x = 0; x < VGA_WIDTH; x++) {
		const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
		t_buffer[index] = make_vgaentry(' ', t_color);
	}
}

void t_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++) t_putchar(data[i]);
}

void t_writestring(const char* data) 
{ 
	t_write(data, strlen(data)); 
}

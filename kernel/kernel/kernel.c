#include <stdio.h>

#include <kernel/tty.h>

void kernel_main(void) {
	terminal_initialize();
	t_writestring("Hello, kernel World!\n");
}

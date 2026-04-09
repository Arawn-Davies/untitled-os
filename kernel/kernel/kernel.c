#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/descr_tbl.h>
#include <kernel/serial.h>
#include <kernel/timer.h>
#include <kernel/system.h>
#include <kernel/debug.h>

void kernel_main(void)
{
	terminal_initialize();
	init_descriptor_tables();
	init_debug_handlers();
	t_writestring("Hello, kernel World!\n");
	init_serial(COM1);
	Serial_WriteString("Hello, kernel World\n");
	init_timer(50);
}

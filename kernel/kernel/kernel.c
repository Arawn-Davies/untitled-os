#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/descr_tbl.h>
#include <kernel/serial.h>
#include <kernel/timer.h>
#include <kernel/system.h>

void kernel_main(void)
{
	terminal_initialize();
	init_descriptor_tables();
	t_writestring("Hello, kernel World!\n");
	init_serial(COM2);
	Serial_WriteString("Hello, kernel World\n");
	init_timer(50);
}

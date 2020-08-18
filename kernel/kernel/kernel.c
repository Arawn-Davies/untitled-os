#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/serial.h>
#include <kernel/timer.h>

void kernel_main(void)
{
	terminal_initialize();
	gdt_install();
	idt_install();
	t_writestring("Hello, kernel World!\n");
	init_serial(COM2);
	Serial_WriteString("Hello, kernel World\n");
	t_writestring("Wagwan world...");
	init_timer(50);
}

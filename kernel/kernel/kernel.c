#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/descr_tbl.h>
#include <kernel/serial.h>
#include <kernel/timer.h>
#include <kernel/system.h>
#include <kernel/debug.h>
#include <kernel/multiboot.h>
#include <kernel/pmm.h>

#include <kernel/paging.h>

void kernel_main(uint32_t magic, multiboot2_info_t *mbi)
{
	terminal_initialize();
	init_descriptor_tables();
	init_debug_handlers();
	t_writestring("Hello, kernel World!\n");
	pmm_init(magic, mbi);
	paging_init();
	init_serial(COM1);
	Serial_WriteString("Hello, kernel World\n");
	init_timer(50);
}

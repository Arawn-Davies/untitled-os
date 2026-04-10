#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/descr_tbl.h>
#include <kernel/serial.h>
#include <kernel/timer.h>
#include <kernel/system.h>
#include <kernel/debug.h>
#include <kernel/multiboot.h>
#include <kernel/pmm.h>
#include <kernel/vesa.h>
#include <kernel/vesa_tty.h>
#include <kernel/heap.h>

#include <kernel/paging.h>
#include <kernel/keyboard.h>
#include <kernel/shell.h>

void kernel_main(uint32_t magic, multiboot2_info_t *mbi)
{
	terminal_initialize();
	t_writestring("Makar kernel starting...\n");

	init_serial(COM1);
	KLOG("serial: COM1 ready\n");
	t_writestring("[ OK ] Serial COM1\n");

	init_descriptor_tables();
	KLOG("gdt/idt: descriptor tables loaded\n");
	t_writestring("[ OK ] Descriptor tables\n");

	init_debug_handlers();
	KLOG("debug: exception handlers registered\n");
	t_writestring("[ OK ] Debug handlers\n");

	pmm_init(magic, mbi);
	t_writestring("[ OK ] Physical memory manager\n");

	paging_init();
	KLOG("paging: init complete\n");
	t_writestring("[ OK ] Paging\n");

	heap_init();
	t_writestring("[ OK ] Heap\n");

	vesa_init(mbi);
	t_writestring("[ OK ] VESA framebuffer\n");

	vesa_tty_init();
	t_writestring("[ OK ] VESA terminal\n");

	init_timer(50);
	KLOG("timer: 50 Hz PIT started\n");
	t_writestring("[ OK ] Timer (50 Hz)\n");

	keyboard_init();
	KLOG("keyboard: PS/2 IRQ1 handler registered\n");
	t_writestring("[ OK ] PS/2 keyboard\n");

	t_writestring("All subsystems ready.\n\n");
	shell_run();
}

#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/vga.h>
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
#include <kernel/ide.h>
#include <kernel/shell.h>

/*
 * Column at which " [ OK ]" is printed.  Labels shorter than this are padded
 * with spaces so the brackets are tab-aligned on every boot line.
 */
#define BOOT_OK_COL 55

/*
 * kprint_ok – finish a boot-step line with dot padding and a green "[ OK ]".
 *
 * Call AFTER writing the step label (without a newline) and AFTER the
 * corresponding init function returns.
 *
 * label   – the same string that was just printed; used to measure its length
 *           so the dots can be padded to BOOT_OK_COL.
 */
static void kprint_ok(const char *label)
{
	/* Measure label length without relying on a libc strlen prototype. */
	size_t len = 0;
	while (label[len])
		len++;

	/* Pad with spaces to reach BOOT_OK_COL (skip if label overruns). */
	for (size_t i = len; i < BOOT_OK_COL; i++)
		t_putchar(' ');

	/* Switch to bright green on both VGA and VESA (if active). */
	t_setcolor(make_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
	if (vesa_tty_is_ready())
		vesa_tty_setcolor(0x00FF00, 0x000000);

	t_writestring(" [ OK ]");

	/* Restore white-on-black. */
	t_setcolor(make_color(COLOR_WHITE, COLOR_BLACK));
	if (vesa_tty_is_ready())
		vesa_tty_setcolor(0xFFFFFF, 0x000000);

	t_putchar('\n');
}

void kernel_main(uint32_t magic, multiboot2_info_t *mbi)
{
	const char *step;

	terminal_initialize();
	t_writestring("Makar kernel starting...\n");

	step = "Initializing serial COM1";
	t_writestring(step);
	init_serial(COM1);
	KLOG("serial: COM1 ready\n");
	kprint_ok(step);

	step = "Loading descriptor tables";
	t_writestring(step);
	init_descriptor_tables();
	KLOG("gdt/idt: descriptor tables loaded\n");
	kprint_ok(step);

	step = "Installing exception handlers";
	t_writestring(step);
	init_debug_handlers();
	KLOG("debug: exception handlers registered\n");
	kprint_ok(step);

	step = "Initializing physical memory";
	t_writestring(step);
	pmm_init(magic, mbi);
	kprint_ok(step);

	step = "Enabling paging";
	t_writestring(step);
	paging_init();
	KLOG("paging: init complete\n");
	kprint_ok(step);

	step = "Initializing heap";
	t_writestring(step);
	heap_init();
	kprint_ok(step);

	step = "Initializing VESA framebuffer";
	t_writestring(step);
	vesa_init(mbi);
	kprint_ok(step);

	step = "Initializing VESA terminal";
	t_writestring(step);
	vesa_tty_init();
	kprint_ok(step);

	step = "Starting timer (50 Hz)";
	t_writestring(step);
	init_timer(50);
	KLOG("timer: 50 Hz PIT started\n");
	kprint_ok(step);

	step = "Registering PS/2 keyboard";
	t_writestring(step);
	keyboard_init();
	KLOG("keyboard: PS/2 IRQ1 handler registered\n");
	kprint_ok(step);

	step = "Initializing IDE controller";
	t_writestring(step);
	ide_init();
	KLOG("ide: ATA PIO scan complete\n");
	kprint_ok(step);

	t_writestring("\nAll subsystems ready.\n\n");
	shell_run();
}

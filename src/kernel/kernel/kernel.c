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
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/acpi.h>

/*
 * Column at which "[ OK ]" is printed, counting from 0.
 * On an 80-column VGA terminal this places the 7-character " [ OK ]" at
 * columns 72–78, leaving column 79 free for the spinner.
 */
#define BOOT_OK_COL (VGA_WIDTH - 8)

/*
 * kprint_ok – finish a boot-step line with padding and a green "[ OK ]".
 *
 * Uses the actual cursor column (t_column) rather than strlen(label) so that
 * output from init functions does not misalign the status badge.
 *
 * If the cursor is already past BOOT_OK_COL (because the init function
 * printed a long message) the text wraps to the next line before printing
 * the badge, keeping it in the same column every time.
 */
static void kprint_ok(void)
{
	/* Wrap to a fresh line if intermediate output ran past the badge column. */
	if (t_column > BOOT_OK_COL)
		t_putchar('\n');

	/* Pad from current column to BOOT_OK_COL. */
	while (t_column < BOOT_OK_COL)
		t_putchar(' ');

	/* Switch to bright green on both VGA and VESA (if active). */
	t_setcolor(make_color(COLOR_LIGHT_GREEN, COLOR_BLACK));
	if (vesa_tty_is_ready())
		vesa_tty_setcolor(0x00FF00, 0x000000);

	t_writestring("[ OK ]");

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
	kprint_ok();

	step = "Loading descriptor tables";
	t_writestring(step);
	init_descriptor_tables();
	KLOG("gdt/idt: descriptor tables loaded\n");
	kprint_ok();

	step = "Installing exception handlers";
	t_writestring(step);
	init_debug_handlers();
	KLOG("debug: exception handlers registered\n");
	kprint_ok();

	step = "Initializing physical memory";
	t_writestring(step);
	pmm_init(magic, mbi);
	kprint_ok();

	step = "Enabling paging (256 MiB, 4 MiB large pages)";
	t_writestring(step);
	paging_init();
	KLOG("paging: init complete\n");
	kprint_ok();

	step = "Initializing heap";
	t_writestring(step);
	heap_init();
	kprint_ok();

	step = "Initializing VESA framebuffer";
	t_writestring(step);
	vesa_init(mbi);
	kprint_ok();

	step = "Initializing VESA terminal";
	t_writestring(step);
	vesa_tty_init();
	kprint_ok();

	step = "Starting timer (50 Hz)";
	t_writestring(step);
	init_timer(50);
	KLOG("timer: 50 Hz PIT started\n");
	kprint_ok();

	step = "Registering PS/2 keyboard";
	t_writestring(step);
	keyboard_init();
	KLOG("keyboard: PS/2 IRQ1 handler registered\n");
	kprint_ok();

	step = "Initializing IDE controller";
	t_writestring(step);
	ide_init();
	KLOG("ide: ATA PIO scan complete\n");
	kprint_ok();

	t_writestring("\nAll subsystems ready.\n\n");

	step = "Initializing multitasking";
	t_writestring(step);
	tasking_init();
	task_create("shell", shell_run);
	kprint_ok();

	step = "Initializing syscalls (int 0x80)";
	t_writestring(step);
	syscall_init();
	kprint_ok();

	step = "Initializing ACPI";
	t_writestring(step);
	acpi_init();
	kprint_ok();

	/*
	 * Transfer control to the scheduler.  The shell task starts running and
	 * the idle task (this context) resumes here whenever no other task is
	 * runnable.  We simply keep yielding and halting between ticks so that
	 * the CPU is not needlessly busy.
	 */
	for (;;) {
		task_yield();
		asm volatile("hlt");
	}
}

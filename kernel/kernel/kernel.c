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

#include <kernel/paging.h>

/* Post-boot heartbeat: prints the tick count 10 times (1 second apart) to
   both the VGA terminal and the serial port.  This confirms that the PIT
   timer interrupt is firing and the OS has not crashed after initialisation.
   The function is non-static so the GDB test can set a breakpoint on it. */
void kernel_post_boot(void)
{
	for (int i = 1; i <= 10; i++) {
		ksleep(50);  /* sleep 50 ticks = 1 s at 50 Hz */
		uint32_t t = timer_get_ticks();
		t_writestring("tick: ");
		t_dec(t);
		t_writestring("\n");
		Serial_WriteString("tick: ");
		Serial_WriteDec(t);
		Serial_WriteString("\n");
	}
}

void kernel_main(uint32_t magic, multiboot2_info_t *mbi)
{
	terminal_initialize();
	init_descriptor_tables();
	init_debug_handlers();
	t_writestring("Hello, kernel World!\n");
	pmm_init(magic, mbi);
	paging_init();
	vesa_init(mbi);
	init_serial(COM1);
	Serial_WriteString("Hello, kernel World\n");
	init_timer(50);
	kernel_post_boot();
}

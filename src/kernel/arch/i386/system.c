#include <kernel/system.h>
#include <kernel/types.h>
#include <kernel/tty.h>

/* halt() — spin with interrupts disabled after an unrecoverable fault. */
inline void halt()
{
	asm volatile ("hlt");
}

void panic(char* message, char* file, uint32_t line)
{
	asm volatile ("cli");
	t_writestring("\nPANIC(");
	t_writestring(message);
	t_writestring(") at ");
	t_writestring(file);
	t_writestring(":");
	t_dec(line);
	t_putchar('\n');
	for(;;);
}

void panic_assert(char *file, uint32_t line, char *desc)
{
	asm volatile("cli");

	t_writestring("ASSERTION-FAILED(");
	t_writestring(desc);
	t_writestring(") at ");
	t_writestring(file);
	t_writestring(":");
	t_dec(line);
	t_putchar('\n');
	for(;;);
}

// debug.c -- Handlers for INT 1 (debug exception) and INT 3 (breakpoint).
// When running under QEMU with the GDB stub (-s), QEMU intercepts these
// exceptions at the hardware-emulation layer and forwards them to the
// attached debugger.  These handlers act as a fallback: if the exceptions
// reach the kernel (e.g. no debugger attached, or a kernel-triggered int3)
// they print a concise register dump to both the VGA terminal and the serial
// port, then return gracefully instead of panicking.

#include <kernel/debug.h>
#include <kernel/isr.h>
#include <kernel/tty.h>
#include <kernel/serial.h>

// ---------------------------------------------------------------------------
// Internal helpers – write a hex value to both outputs
// ---------------------------------------------------------------------------

static void write_hex_both(uint32_t val)
{
    t_writestring("0x");
    t_hex(val);
    // Serial has no hex helper, build it manually
    static const char hexdigits[] = "0123456789ABCDEF";
    char buf[11]; // "0x" + 8 hex digits + NUL
    int digit_index;
    buf[0] = '0';
    buf[1] = 'x';
    for (digit_index = 7; digit_index >= 0; digit_index--) {
        buf[2 + digit_index] = hexdigits[val & 0xF];
        val >>= 4;
    }
    buf[10] = '\0';
    Serial_WriteString(buf);
}

static void write_reg(const char *name, uint32_t val)
{
    t_writestring(name);
    t_writestring("=");
    write_hex_both(val);
    t_writestring("  ");
    Serial_WriteString("\n");
}

// ---------------------------------------------------------------------------
// Dump the most useful registers from a trap frame
// ---------------------------------------------------------------------------

static void dump_registers(registers_t *regs)
{
    t_writestring("\n--- register dump ---\n");
    Serial_WriteString("--- register dump ---\n");
    write_reg("EIP   ", regs->eip);
    write_reg("EFLAGS", regs->eflags);
    write_reg("EAX   ", regs->eax);
    write_reg("EBX   ", regs->ebx);
    write_reg("ECX   ", regs->ecx);
    write_reg("EDX   ", regs->edx);
    write_reg("ESI   ", regs->esi);
    write_reg("EDI   ", regs->edi);
    write_reg("EBP   ", regs->ebp);
    write_reg("ESP   ", regs->esp);
    write_reg("CS    ", regs->cs);
    write_reg("SS    ", regs->ss);
    t_writestring("---------------------\n");
    Serial_WriteString("---------------------\n");
}

// ---------------------------------------------------------------------------
// INT 1 – Debug exception (single-step / hardware breakpoint)
// ---------------------------------------------------------------------------

static void debug_exception_handler(registers_t regs)
{
    t_writestring("[DEBUG] INT1 debug exception\n");
    Serial_WriteString("[DEBUG] INT1 debug exception\n");
    dump_registers(&regs);
    // Trap — execution resumes at the next instruction automatically.
}

// ---------------------------------------------------------------------------
// INT 3 – Breakpoint (software int3 / GDB software breakpoint)
// ---------------------------------------------------------------------------

static void breakpoint_handler(registers_t regs)
{
    t_writestring("[DEBUG] INT3 breakpoint hit\n");
    Serial_WriteString("[DEBUG] INT3 breakpoint hit\n");
    dump_registers(&regs);
    // Trap — execution resumes after the int3 instruction automatically.
}

// ---------------------------------------------------------------------------
// Public initialisation – called from kernel_main
// ---------------------------------------------------------------------------

void init_debug_handlers()
{
    register_interrupt_handler(1, debug_exception_handler);
    register_interrupt_handler(3, breakpoint_handler);
}

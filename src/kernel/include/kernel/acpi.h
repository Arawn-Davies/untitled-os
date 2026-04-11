#ifndef _KERNEL_ACPI_H
#define _KERNEL_ACPI_H

#include <stdint.h>

/*
 * acpi_init – scan for the RSDP and parse the FADT / DSDT.
 *
 * Must be called after paging is enabled (BIOS ROM is identity-mapped in the
 * first 8 MiB window).  On success the PM1a/PM1b control-block ports and the
 * S5 sleep-type values are cached for use by acpi_shutdown().
 *
 * Returns 1 if ACPI tables were found and parsed successfully, 0 otherwise.
 */
int acpi_init(void);

/*
 * acpi_shutdown – power off the machine via ACPI S5 ("soft off").
 *
 * Tries (in order):
 *   1. ACPI PM1 control registers (if acpi_init() succeeded).
 *   2. QEMU/Bochs I/O-port fallbacks (0x604 / 0xB004).
 *   3. cli + hlt spin loop (last resort – machine appears "frozen").
 *
 * This function never returns.
 */
__attribute__((noreturn)) void acpi_shutdown(void);

/*
 * acpi_reboot – reset the machine.
 *
 * Tries (in order):
 *   1. ACPI reset register (FADT revision >= 2, I/O port variant).
 *   2. PS/2 keyboard controller CPU-reset pulse (port 0x64, command 0xFE).
 *   3. Triple-fault: load a zero-limit IDT and fire int $0.
 *
 * This function never returns.
 */
__attribute__((noreturn)) void acpi_reboot(void);

#endif /* _KERNEL_ACPI_H */

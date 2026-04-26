#ifndef _KERNEL_CHAINLOAD_H
#define _KERNEL_CHAINLOAD_H

#include <kernel/types.h>

/*
 * chainload_enter – disable paging, return to real mode, jump to 0x7C00.
 *
 * The caller must copy a valid 512-byte boot sector (ending 0x55 0xAA) to
 * physical address 0x7C00 before calling this function.
 *
 * dl: BIOS drive number passed to the boot sector in DL.
 *     0x80 = first HDD, 0x81 = second HDD, 0x00 = first floppy.
 *
 * Never returns.
 */
__attribute__((noreturn)) void chainload_enter(uint8_t dl);

#endif /* _KERNEL_CHAINLOAD_H */

/*
 * linker.ld -- linker script for Makar i386 userspace ELF programs.
 *
 * Programs are linked to load at 0x08048000, the traditional i386 ELF
 * user-space base address.  The ELF loader will map each PT_LOAD segment
 * at the virtual address recorded in the program headers; this script
 * ensures those addresses are reasonable.
 *
 * Keep this script minimal — only the sections actually produced by GCC
 * for freestanding C programs need to be listed.
 */

ENTRY(_start)

SECTIONS
{
    /* Traditional i386 ELF user-space base address. */
    . = 0x08048000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
}

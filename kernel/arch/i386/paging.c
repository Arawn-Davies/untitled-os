#include <kernel/paging.h>
#include <kernel/isr.h>
#include <kernel/tty.h>
#include <kernel/system.h>

/* 8 MiB / 4 KiB per page = 2048 pages = 2 page tables (1024 entries each) */
#define PAGE_TABLES_8MB  2

/* Page entry flags */
#define PAGE_PRESENT   0x1
#define PAGE_WRITABLE  0x2
/* No USER bit → supervisor-only */

/* Static, page-aligned structures in BSS.
   The linker places these inside the kernel image, so they are already
   covered by the identity mapping we are about to install. */
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[PAGE_TABLES_8MB][1024] __attribute__((aligned(4096)));

/* ISR 14 – Page-fault handler.
   CR2 holds the linear address that caused the fault. */
static void page_fault_handler(registers_t regs)
{
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

    t_writestring("Page fault at 0x");
    t_hex(faulting_address);
    t_writestring(" (err=0x");
    t_hex(regs.err_code);
    t_writestring(")\n");

    PANIC("Page fault");
}

void paging_init(void)
{
    /* Fill page tables: identity-map 0–8 MiB, supervisor-only, writable. */
    for (uint32_t pt = 0; pt < PAGE_TABLES_8MB; pt++) {
        for (uint32_t page = 0; page < 1024; page++) {
            uint32_t phys = (pt * 1024 + page) * 0x1000;
            page_tables[pt][page] = phys | PAGE_PRESENT | PAGE_WRITABLE;
        }
    }

    /* Wire the two page tables into the page directory. */
    for (uint32_t pt = 0; pt < PAGE_TABLES_8MB; pt++) {
        page_directory[pt] = (uint32_t)page_tables[pt] | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* Register the page-fault handler (ISR 14). */
    register_interrupt_handler(14, page_fault_handler);

    /* Load CR3 with the physical address of the page directory. */
    asm volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");

    /* Enable paging: set CR0.PG (bit 31). */
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");

    t_writestring("Paging: enabled (identity-mapped 0-8 MB)\n");
}

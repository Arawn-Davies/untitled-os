# paging — Paging and virtual memory

**Header:** `kernel/include/kernel/paging.h`  
**Source:** `kernel/arch/i386/paging.c`

Enables the x86 32-bit paging unit and provides a runtime region-mapping
function used by the heap and VESA framebuffer drivers to access memory
outside the initial 8 MiB boot window.

---

## Boot-time identity map

`paging_init()` identity-maps the first **8 MiB** of physical memory
(physical address = virtual address) using two pre-allocated page tables of
1 024 entries each.  All pages in this range are supervisor-only and
writable.

This window covers:

- The kernel image (loaded at 1 MiB by GRUB).
- The VGA text buffer (`0xB8000`).
- The PMM bitmap, page tables, and other BSS/data.

---

## Page-table pool

A static pool of 32 extra page tables (`extra_page_tables`) handles
post-boot mapping requests.  Each page table covers 4 MiB of virtual address
space, so the pool can map up to **128 MiB** of additional regions.  This is
enough for the 16 MiB kernel heap and the typical VESA framebuffer.

---

## Functions

### `paging_init`

```c
void paging_init(void);
```

1. Fill two pre-allocated page tables with identity-map entries for 0–8 MiB.
2. Wire the page tables into the page directory.
3. Register `page_fault_handler` on ISR 14.
4. Load CR3 with the physical address of the page directory.
5. Set CR0 bit 31 (PG) to enable paging.

### `paging_map_region`

```c
void paging_map_region(uint32_t phys_start, uint32_t size);
```

Identity-map the physical address range `[phys_start, phys_start + size)`.
Pages already mapped are silently skipped.  If the page-table pool is
exhausted the function returns without mapping anything further.

| Parameter | Description |
|---|---|
| `phys_start` | Physical start address. Need not be page-aligned; the function rounds down. |
| `size` | Length in bytes. Need not be page-aligned; the function rounds up. |

Called by:
- `heap_init()` — to map the 16 MiB heap region starting at 8 MiB.
- `vesa_tty_init()` — to map the VESA linear framebuffer before the first pixel write.

---

## Page-fault handler

A simple panic handler is installed on ISR 14.  It reads the faulting address
from CR2, prints it alongside the error code, then calls `PANIC("Page fault")`.

The error code bits indicate:
- Bit 0: 0 = not-present, 1 = protection violation.
- Bit 1: 0 = read, 1 = write.
- Bit 2: 0 = supervisor, 1 = user mode.

---

## Future work

- Separate kernel and user address spaces (higher-half kernel or full
  split-address mapping).
- Demand paging: swap absent pages in from a backing store.
- Copy-on-write support for future process forking.
- A proper TLB shootdown mechanism for SMP.
- Guard pages around the kernel stack to catch stack overflows early.

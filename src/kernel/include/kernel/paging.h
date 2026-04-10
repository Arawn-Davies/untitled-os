#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <stdint.h>

/* Set up paging:
 *   - identity-map the first 8 MiB (supervisor-only, writable)
 *   - register a page-fault handler (ISR 14) that panics with the faulting address
 *   - enable paging by loading CR3 and setting CR0.PG
 */
void paging_init(void);

/*
 * Identity-map an arbitrary physical address range [phys_start, phys_start+size)
 * using a static pool of extra page tables.  Safe to call after paging_init().
 * Silently ignores ranges that are already mapped.  Returns without mapping
 * anything if the internal page-table pool is exhausted.
 */
void paging_map_region(uint32_t phys_start, uint32_t size);

#endif /* _KERNEL_PAGING_H */

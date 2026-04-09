#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <stdint.h>

/* Set up paging:
 *   - identity-map the first 8 MiB (supervisor-only, writable)
 *   - register a page-fault handler (ISR 14) that panics with the faulting address
 *   - enable paging by loading CR3 and setting CR0.PG
 */
void paging_init(void);

#endif /* _KERNEL_PAGING_H */

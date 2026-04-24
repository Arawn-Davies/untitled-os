/*
 * ktest.c -- In-kernel unit test runner.
 *
 * Each suite is a static function called from ktest_run_all().  Results are
 * written directly to the VGA terminal so they work without heap or FS.
 */

#include <kernel/ktest.h>
#include <kernel/acpi.h>
#include <kernel/partition.h>
#include <kernel/pmm.h>
#include <kernel/heap.h>
#include <kernel/vmm.h>
#include <kernel/paging.h>
#include <kernel/tty.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Harness state & primitives
 * ------------------------------------------------------------------------- */

int ktest_pass_count = 0;
int ktest_fail_count = 0;

void ktest_begin(const char *suite)
{
    ktest_pass_count = 0;
    ktest_fail_count = 0;
    t_writestring("\n[ktest] suite: ");
    t_writestring(suite);
    t_putchar('\n');
}

void ktest_assert(int cond, const char *expr, const char *file, uint32_t line)
{
    if (cond) {
        t_writestring("  PASS: ");
        t_writestring(expr);
        t_putchar('\n');
        ktest_pass_count++;
    } else {
        t_writestring("  FAIL: ");
        t_writestring(expr);
        t_writestring("  (");
        t_writestring(file);
        t_putchar(':');
        t_dec(line);
        t_writestring(")\n");
        ktest_fail_count++;
    }
}

void ktest_summary(void)
{
    t_writestring("[ktest] results: ");
    t_dec((uint32_t)ktest_pass_count);
    t_writestring(" passed, ");
    t_dec((uint32_t)ktest_fail_count);
    t_writestring(" failed\n");
}

/* ---------------------------------------------------------------------------
 * Suite: ACPI helpers
 *
 * acpi_checksum() is the only internal helper we can call from outside
 * acpi.c without touching real hardware.  We test it with known buffers.
 * ------------------------------------------------------------------------- */

static void test_acpi_checksum(void)
{
    ktest_begin("acpi_checksum");

    /* A buffer whose byte sum is 0 — valid. */
    uint8_t good[4] = {0x01, 0x02, 0x03, 0xFA}; /* 1+2+3+250 = 256 → 0 mod 256 */
    KTEST_ASSERT(acpi_checksum(good, 4));

    /* Off-by-one: change the last byte so the sum is non-zero. */
    uint8_t bad[4] = {0x01, 0x02, 0x03, 0xFB};
    KTEST_ASSERT(!acpi_checksum(bad, 4));

    /* Single byte whose value is 0 — valid (sum = 0). */
    uint8_t zero[1] = {0x00};
    KTEST_ASSERT(acpi_checksum(zero, 1));

    /* Single byte whose value is non-zero — invalid. */
    uint8_t nonzero[1] = {0x01};
    KTEST_ASSERT(!acpi_checksum(nonzero, 1));

    /* Empty buffer (length 0) — sum is 0, always valid. */
    KTEST_ASSERT(acpi_checksum(good, 0));

    ktest_summary();
}

/* ---------------------------------------------------------------------------
 * Suite: string helpers (sanity-check the libc stubs used by the kernel)
 * ------------------------------------------------------------------------- */

static void test_string(void)
{
    ktest_begin("string");

    KTEST_ASSERT(strlen("hello") == 5);
    KTEST_ASSERT(strlen("") == 0);

    KTEST_ASSERT(strcmp("abc", "abc") == 0);
    KTEST_ASSERT(strcmp("abc", "abd") < 0);
    KTEST_ASSERT(strcmp("abd", "abc") > 0);

    KTEST_ASSERT(strncmp("abcX", "abcY", 3) == 0);
    KTEST_ASSERT(strncmp("abcX", "abcY", 4) != 0);

    char buf[16];
    memset(buf, 0xAA, sizeof(buf));
    KTEST_ASSERT((uint8_t)buf[0] == 0xAA);

    memcpy(buf, "hello", 6);
    KTEST_ASSERT(strcmp(buf, "hello") == 0);

    ktest_summary();
}

/* ---------------------------------------------------------------------------
 * Suite: partition helpers
 *
 * Tests the pure-logic helpers in partition.c that can run without hardware.
 * part_type_name() and part_guid_type_name() both look up tables in BSS,
 * so they work without any disk being present.
 * ------------------------------------------------------------------------- */

static void test_partition(void)
{
    ktest_begin("partition");

    /* MBR type name lookup */
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_EMPTY),     "Empty")          == 0);
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_FAT32_LBA), "FAT32 (LBA)")    == 0);
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_FAT32_CHS), "FAT32 (CHS)")    == 0);
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_GPT_PROT),  "GPT protective") == 0);
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_EFI),       "EFI System")     == 0);
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_MDFS),      "MDFS")           == 0);
    KTEST_ASSERT(strcmp(part_type_name(PART_MBR_LINUX),     "Linux")          == 0);
    /* Unknown type returns a non-empty string (not NULL). */
    KTEST_ASSERT(part_type_name(0xAB) != 0);

    /* GPT GUID type name lookup */
    KTEST_ASSERT(strcmp(part_guid_type_name(PART_GUID_FAT32), "FAT32")      == 0);
    KTEST_ASSERT(strcmp(part_guid_type_name(PART_GUID_EFI),   "EFI System") == 0);
    KTEST_ASSERT(strcmp(part_guid_type_name(PART_GUID_LINUX), "Linux Data") == 0);
    KTEST_ASSERT(strcmp(part_guid_type_name(PART_GUID_MDFS),  "MDFS")       == 0);

    /* All-zero GUID is "Unused". */
    static const uint8_t zero16[16] = {0};
    KTEST_ASSERT(strcmp(part_guid_type_name(zero16), "Unused") == 0);

    /* An unrecognised GUID returns a non-NULL string. */
    static const uint8_t unknown[16] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE,
        0xBA, 0xBE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    KTEST_ASSERT(part_guid_type_name(unknown) != 0);

    ktest_summary();
}



/* ---------------------------------------------------------------------------
 * Suite: PMM
 *
 * Exercises pmm_alloc_frame / pmm_free_frame using the live allocator.
 * All allocated frames are freed before the suite returns so the PMM state
 * is identical before and after.
 * ------------------------------------------------------------------------- */

static void test_pmm(void)
{
    ktest_begin("pmm");

    /* Alloc must return a 4 KiB-aligned non-error address. */
    uint32_t f1 = pmm_alloc_frame();
    KTEST_ASSERT(f1 != PMM_ALLOC_ERROR);
    KTEST_ASSERT((f1 & 0xFFFu) == 0);

    /* Two consecutive allocs must return distinct frames. */
    uint32_t f2 = pmm_alloc_frame();
    KTEST_ASSERT(f2 != PMM_ALLOC_ERROR);
    KTEST_ASSERT(f1 != f2);

    /* free_count decreases by 1 per alloc. */
    uint32_t fc = pmm_free_count();
    uint32_t f3 = pmm_alloc_frame();
    KTEST_ASSERT(f3 != PMM_ALLOC_ERROR);
    KTEST_ASSERT(pmm_free_count() == fc - 1);

    /* Freeing a frame increments free_count and makes it re-allocatable. */
    pmm_free_frame(f3);
    KTEST_ASSERT(pmm_free_count() == fc);
    uint32_t f4 = pmm_alloc_frame();
    KTEST_ASSERT(f4 == f3);   /* same frame recycled (first-fit scan) */

    pmm_free_frame(f1);
    pmm_free_frame(f2);
    pmm_free_frame(f4);

    ktest_summary();
}

/* ---------------------------------------------------------------------------
 * Suite: heap
 *
 * Tests the kmalloc/kfree/krealloc first-fit allocator.
 * ------------------------------------------------------------------------- */

static void test_heap(void)
{
    ktest_begin("heap");

    /* kmalloc(0) must return NULL. */
    KTEST_ASSERT(kmalloc(0) == NULL);

    /* Normal allocations return non-NULL distinct pointers. */
    void *p1 = kmalloc(64);
    void *p2 = kmalloc(64);
    KTEST_ASSERT(p1 != NULL);
    KTEST_ASSERT(p2 != NULL);
    KTEST_ASSERT(p1 != p2);

    /* Data written to one block is not clobbered by the other. */
    memset(p1, 0xAB, 64);
    memset(p2, 0xCD, 64);
    KTEST_ASSERT(((uint8_t *)p1)[0]  == 0xAB);
    KTEST_ASSERT(((uint8_t *)p1)[63] == 0xAB);
    KTEST_ASSERT(((uint8_t *)p2)[0]  == 0xCD);

    /* kfree(NULL) must not crash. */
    kfree(NULL);

    /* krealloc to a larger size preserves existing bytes. */
    void *p3 = kmalloc(16);
    KTEST_ASSERT(p3 != NULL);
    memset(p3, 0xEF, 16);
    void *p4 = krealloc(p3, 128);
    KTEST_ASSERT(p4 != NULL);
    KTEST_ASSERT(((uint8_t *)p4)[0]  == 0xEF);
    KTEST_ASSERT(((uint8_t *)p4)[15] == 0xEF);

    /* krealloc(ptr, 0) behaves like kfree and returns NULL. */
    void *p5 = kmalloc(32);
    KTEST_ASSERT(p5 != NULL);
    void *p6 = krealloc(p5, 0);
    KTEST_ASSERT(p6 == NULL);

    /* After freeing, the allocator can hand out a new block in that space. */
    kfree(p1);
    void *p7 = kmalloc(64);
    KTEST_ASSERT(p7 != NULL);

    kfree(p2);
    kfree(p4);
    kfree(p7);

    ktest_summary();
}

/* ---------------------------------------------------------------------------
 * Suite: VMM
 *
 * Tests vmm_create_pd / vmm_map_page / vmm_unmap_page / vmm_free_pd.
 *
 * Page table entries are inspected directly: since the kernel is identity-
 * mapped (phys == virt), every PMM frame address is directly dereferenceable
 * as a uint32_t pointer.
 *
 * x86 page-entry bit positions used here:
 *   bit 0 (0x1)  – Present
 *   bit 7 (0x80) – PS / large page (set in kernel PSE entries)
 * ------------------------------------------------------------------------- */

#define KT_PAGE_PRESENT  0x1u
#define KT_PAGE_LARGE    0x80u

static void test_vmm(void)
{
    ktest_begin("vmm");

    uint32_t *kpd = paging_kernel_pd();

    /* vmm_create_pd returns a 4 KiB-aligned pointer. */
    uint32_t *pd = vmm_create_pd();
    KTEST_ASSERT(pd != NULL);
    KTEST_ASSERT(((uint32_t)pd & 0xFFFu) == 0);

    /* Kernel PDEs (indices 0–63) are propagated into the new PD. */
    KTEST_ASSERT(pd[0]  == kpd[0]);
    KTEST_ASSERT(pd[32] == kpd[32]);
    KTEST_ASSERT(pd[63] == kpd[63]);

    /* Non-kernel PDEs are zeroed. */
    KTEST_ASSERT((pd[64]   & KT_PAGE_PRESENT) == 0);
    KTEST_ASSERT((pd[1023] & KT_PAGE_PRESENT) == 0);

    /* vmm_map_page: allocate a frame and map it at a user virtual address. */
    uint32_t phys = pmm_alloc_frame();
    KTEST_ASSERT(phys != PMM_ALLOC_ERROR);

    uint32_t virt = 0x40001000u;               /* PDE 256, PTE 1 */
    uint32_t pdi  = virt >> 22;                /* 256             */
    uint32_t pti  = (virt >> 12) & 0x3FFu;    /* 1               */

    vmm_map_page(pd, virt, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);

    /* PDE for the mapped region must be present and NOT a large page. */
    KTEST_ASSERT((pd[pdi] & KT_PAGE_PRESENT) != 0);
    KTEST_ASSERT((pd[pdi] & KT_PAGE_LARGE)   == 0);

    /* The PTE must point to the right physical frame with Present set. */
    uint32_t *pt = (uint32_t *)(pd[pdi] & ~0xFFFu);
    KTEST_ASSERT((pt[pti] & KT_PAGE_PRESENT) != 0);
    KTEST_ASSERT((pt[pti] & ~0xFFFu) == phys);

    /* Mapping inside the kernel large-page window is silently ignored. */
    vmm_map_page(pd, 0x1000u, phys, VMM_FLAG_USER);  /* PDE 0 = large page */
    KTEST_ASSERT(pd[0] == kpd[0]);                    /* PDE 0 unchanged    */

    /* vmm_unmap_page clears the PTE (does not free the frame). */
    vmm_unmap_page(pd, virt);
    KTEST_ASSERT((pt[pti] & KT_PAGE_PRESENT) == 0);

    /* Re-map so vmm_free_pd has a frame to release. */
    vmm_map_page(pd, virt, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);

    /* vmm_free_pd releases the mapped frame, the page table, and the PD itself.
     * Three PMM frames total must be returned. */
    uint32_t fc_before = pmm_free_count();
    vmm_free_pd(pd);                           /* frees phys + PT + PD = 3 */
    KTEST_ASSERT(pmm_free_count() == fc_before + 3);

    ktest_summary();
}

void ktest_run_all(void)
{
    int total_pass = 0;
    int total_fail = 0;

    test_acpi_checksum();
    total_pass += ktest_pass_count;
    total_fail += ktest_fail_count;

    test_string();
    total_pass += ktest_pass_count;
    total_fail += ktest_fail_count;

    test_partition();
    total_pass += ktest_pass_count;
    total_fail += ktest_fail_count;

    test_pmm();
    total_pass += ktest_pass_count;
    total_fail += ktest_fail_count;

    test_heap();
    total_pass += ktest_pass_count;
    total_fail += ktest_fail_count;

    test_vmm();
    total_pass += ktest_pass_count;
    total_fail += ktest_fail_count;

    t_writestring("\n[ktest] TOTAL: ");
    t_dec((uint32_t)total_pass);
    t_writestring(" passed, ");
    t_dec((uint32_t)total_fail);
    t_writestring(" failed\n");
}

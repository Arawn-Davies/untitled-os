/*
 * ktest.c -- In-kernel unit test runner.
 *
 * Each suite is a static function called from ktest_run_all().  Results are
 * written directly to the VGA terminal so they work without heap or FS.
 */

#include <kernel/ktest.h>
#include <kernel/acpi.h>
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
 * Public entry point
 * ------------------------------------------------------------------------- */

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

    t_writestring("\n[ktest] TOTAL: ");
    t_dec((uint32_t)total_pass);
    t_writestring(" passed, ");
    t_dec((uint32_t)total_fail);
    t_writestring(" failed\n");
}

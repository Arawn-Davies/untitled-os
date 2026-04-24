/*
 * usertest.c — minimal ring-3 smoke test.
 *
 * Spawns a kernel task that builds a one-page user address space, copies a
 * tiny position-independent binary into it, and drops to ring 3 via
 * ring3_enter().  The user code calls SYS_WRITE to print a greeting and then
 * SYS_EXIT to terminate cleanly.
 */

#include <kernel/ring3.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/task.h>
#include <kernel/descr_tbl.h>
#include <kernel/tty.h>
#include <string.h>

/* User virtual address layout.
 * Both addresses sit above the 256 MiB kernel identity window (PDE 64+). */
#define USER_CODE_BASE   0x40000000u   /* user code mapped here         */
#define USER_STACK_TOP   0xBFFF0000u   /* user stack top; one page below */

/*
 * Position-independent i386 machine code — equivalent to:
 *
 *   write("Welcome to userspace!\n");
 *   exit(0);
 *
 * The call/pop idiom makes it load-address-independent: after the call the
 * return address on the stack equals the runtime address of the pop, giving
 * a base from which the msg offset is computed.
 *
 * Disassembly (offsets from code start):
 *   00: E8 00 00 00 00   call  +0       ; push &pop_esi, jmp to pop_esi
 *   05: 5E               pop   esi      ; esi = runtime addr of this insn
 *   06: 83 C6 16         add   esi, 22  ; esi = msg  (22 = 27 - 5)
 *   09: B8 04 00 00 00   mov   eax, 4   ; SYS_WRITE
 *   0E: 89 F3            mov   ebx, esi ; EBX = msg ptr
 *   10: CD 80            int   0x80
 *   12: B8 01 00 00 00   mov   eax, 1   ; SYS_EXIT
 *   17: 31 DB            xor   ebx, ebx
 *   19: CD 80            int   0x80
 *   1B: msg              "Welcome to userspace!\n\0"  (offset 27)
 */
static const uint8_t user_test_bin[] = {
    0xE8, 0x00, 0x00, 0x00, 0x00,          /* call  +0          */
    0x5E,                                   /* pop   esi         */
    0x83, 0xC6, 0x16,                       /* add   esi, 22     */
    0xB8, 0x04, 0x00, 0x00, 0x00,          /* mov   eax, 4      */
    0x89, 0xF3,                             /* mov   ebx, esi    */
    0xCD, 0x80,                             /* int   0x80        */
    0xB8, 0x01, 0x00, 0x00, 0x00,          /* mov   eax, 1      */
    0x31, 0xDB,                             /* xor   ebx, ebx   */
    0xCD, 0x80,                             /* int   0x80        */
    /* msg at offset 27: */
    'W','e','l','c','o','m','e',' ',
    't','o',' ',
    'u','s','e','r','s','p','a','c','e','!','\n','\0'
};

static void usertest_task(void)
{
    /* 1. Fresh user address space (kernel PDEs shared). */
    uint32_t *pd = vmm_create_pd();
    if (!pd) {
        t_writestring("ring3test: vmm_create_pd failed\n");
        return;
    }

    /* 2. Code page — user-readable, not writable. */
    uint32_t code_phys = pmm_alloc_frame();
    if (code_phys == PMM_ALLOC_ERROR) {
        t_writestring("ring3test: pmm_alloc_frame failed (code)\n");
        vmm_free_pd(pd);
        return;
    }
    memset((void *)code_phys, 0, PMM_FRAME_SIZE);
    memcpy((void *)code_phys, user_test_bin, sizeof(user_test_bin));
    vmm_map_page(pd, USER_CODE_BASE, code_phys, VMM_FLAG_USER);

    /* 3. Stack page — user-readable and writable. */
    uint32_t stack_phys = pmm_alloc_frame();
    if (stack_phys == PMM_ALLOC_ERROR) {
        t_writestring("ring3test: pmm_alloc_frame failed (stack)\n");
        pmm_free_frame(code_phys);
        vmm_free_pd(pd);
        return;
    }
    memset((void *)stack_phys, 0, PMM_FRAME_SIZE);
    vmm_map_page(pd, USER_STACK_TOP - PMM_FRAME_SIZE,
                 stack_phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);

    /* 4. Activate the user address space and point the TSS at our kernel stack
     *    so that int 0x80 from ring 3 lands in the right place. */
    task_current()->page_dir = pd;
    tss_set_kernel_stack((uint32_t)(task_current()->stack + TASK_STACK_SIZE));
    vmm_switch(pd);

    /* 5. Drop to ring 3 — does not return. */
    ring3_enter(USER_CODE_BASE, USER_STACK_TOP);
}

void cmd_ring3test(void)
{
    t_writestring("Spawning ring-3 test process...\n");
    task_t *t = task_create("ring3test", usertest_task);
    if (!t) {
        t_writestring("ring3test: task_create failed (pool full?)\n");
        return;
    }
    /* Yield so the new task runs before the shell blocks on keyboard input.
       This is a cooperative scheduler — without an explicit yield here the
       ring3test task would never get CPU time until the next keypress. */
    task_yield();
}

/*
 * crt0.S -- C runtime startup for i386 userspace ELF programs.
 *
 * The ELF loader places the standard i386 ABI process stack before jumping
 * here:
 *
 *   [esp+0]   argc  (int)
 *   [esp+4]   argv  (char **)  -- argv[0] through argv[argc-1], then NULL
 *
 * We extract argc and argv, call main(), then invoke SYS_EXIT so the kernel
 * can reclaim the task slot.  If main() returns, the exit code it returns is
 * forwarded as the exit code.
 *
 * Calling convention used by the kernel (Linux i386 ABI):
 *   EAX = syscall number
 *   EBX = arg1  (exit code for SYS_EXIT -- currently ignored by kernel)
 */

.section .text
.global _start
.type   _start, @function

_start:
    /* Ensure the frame pointer is zeroed so stack unwinders stop here. */
    xorl    %ebp, %ebp

    /* Read argc and compute argv from the ELF process stack. */
    movl    (%esp),  %eax       /* argc                */
    leal    4(%esp), %ecx       /* &argv[0]            */

    /* Push arguments for main(argc, argv) in C calling convention. */
    pushl   %ecx                /* argv                */
    pushl   %eax                /* argc                */

    call    main

    /* Forward main()'s return value as the exit code. */
    movl    %eax, %ebx          /* exit code -> EBX    */
    movl    $1,   %eax          /* SYS_EXIT (1) -> EAX */
    int     $0x80

    /* Should never be reached. */
    hlt
.size _start, . - _start

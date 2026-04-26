.section .text
.global _start
_start:
    call    main
    /* main's return value is already in eax; use it as exit status */
    movl    %eax, %ebx
    movl    $1,   %eax      /* SYS_EXIT */
    int     $0x80
    /* unreachable */
1:  jmp 1b

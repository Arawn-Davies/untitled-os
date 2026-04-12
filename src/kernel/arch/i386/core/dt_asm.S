/* Descriptor Table Assembler stubs
 *
 * gdt_flush – reload segment registers after installing a new GDT.
 *   Segment registers DS, ES, FS, GS and SS are set to the kernel data
 *   selector (0x10).  CS cannot be changed with a MOV; a far jump is used
 *   to atomically update CS with the kernel code selector (0x08).
 *   Reference: https://wiki.osdev.org/GDT_Tutorial
 *
 * idt_flush – load the IDTR with the address of our IDT descriptor.
 *   Reference: https://wiki.osdev.org/IDT
 *
 * tss_flush – load the Task Register (TR) with the TSS selector.
 *   GDT[5] holds the TSS descriptor → selector = 5*8 = 0x28.
 *   LTR marks the descriptor as "Busy" in the GDT; the descriptor must
 *   already be marked Available (type 0x9) before LTR is called.
 *   Reference: https://wiki.osdev.org/TSS
 */

.global gdt_flush              /* Allows the C code to call gdt_flush(). */
gdt_flush:
        movl 4(%esp), %eax      /* Get the pointer to the GDT, passed as a parameter. */
        lgdt (%eax)             /* Load the new GDT pointer */

        movw $0x10, %ax         /* 0x10 is the offset in the GDT to our data segment. */
        movw %ax, %ds           /* Load all the data segment selectors. */
        movw %ax, %es
        movw %ax, %fs
        movw %ax, %gs
        movw %ax, %ss
        ljmp $0x08, $.flush     /* 0x08 is the offset to our code segment: Far jump! */
.flush:
        ret

.global idt_flush              /* Allows the C code to call idt_flush(). */
idt_flush:
        movl 4(%esp), %eax      /* Get the pointer to the IDT, passed as a parameter. */
        lidt (%eax)             /* Load the IDT pointer. */
        ret

/*
 * tss_flush – load the Task Register with the TSS selector (0x28).
 *
 * GDT[5] = TSS descriptor → selector = 5*8 = 0x28.
 * LTR requires the descriptor to be marked Available (type 0x9), not Busy.
 * After LTR the CPU updates the descriptor's type field to Busy (0xB).
 * Reference: https://wiki.osdev.org/TSS
 */
.global tss_flush
tss_flush:
        movw $0x28, %ax         /* TSS selector: GDT index 5, TI=0, RPL=0 */
        ltr  %ax
        ret

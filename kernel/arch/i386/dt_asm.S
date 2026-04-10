/* Descriptor Table Assembler source
 *
 * This will set up our new segment registers. We need to do
 * something special in order to set CS. We do what is called a
 * far jump. A jump that includes a segment as well as an offset.
 * This is declared in C as 'extern void gdt_flush(uint32_t gdt_ptr_addr);'
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

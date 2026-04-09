# Multiboot 2 header constants.
.set MB2_MAGIC,        0xE85250D6  # Multiboot 2 magic
.set MB2_ARCH,         0           # i386 protected mode
.set MB2_HEADER_LEN,   (mb2_header_end - mb2_header_start)
.set MB2_CHECKSUM,     -(MB2_MAGIC + MB2_ARCH + MB2_HEADER_LEN)

# Declare a Multiboot 2 header.  Must be 8-byte aligned and within the
# first 32768 bytes of the binary image.
.section .multiboot2
.align 8
mb2_header_start:
.long  MB2_MAGIC
.long  MB2_ARCH
.long  MB2_HEADER_LEN
.long  MB2_CHECKSUM
# End tag (type=0, flags=0, size=8)
.short 0
.short 0
.long  8
mb2_header_end:

# Reserve a stack for the initial thread.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function
_start:
	movl $stack_top, %esp

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	# Pass multiboot2 info: push mbi pointer (ebx) then magic (eax) so
	# kernel_main receives them as (uint32_t magic, multiboot2_info_t *mbi).
	pushl %ebx
	pushl %eax
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size _start, . - _start

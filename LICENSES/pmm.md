# Physical Memory Manager (PMM)

**Files:** `kernel/arch/i386/pmm.c`, `kernel/include/kernel/pmm.h`

## License

The PMM implementation in this project is original work by Arawn Davies and is
distributed under the project's [MIT License](../LICENSE).

## Description

The PMM is a bitmap-based physical frame allocator for 32-bit x86. It manages
up to 4 GiB of address space (1 048 576 frames of 4 KiB each), stored as a
flat 128 KiB bitmap in which a set bit indicates a used frame and a clear bit
indicates a free frame.

Key design points:

- **Initialisation** (`pmm_init`): reads the Multiboot 2 memory-map tag to
  discover usable RAM, marks the null page and all kernel image frames as used.
- **Allocation** (`pmm_alloc_frame`): first-fit linear scan over the bitmap,
  returns the physical address of the allocated frame or `PMM_ALLOC_ERROR`
  (`0xFFFFFFFF`) when no frame is available.
- **Deallocation** (`pmm_free_frame`): clears the corresponding bit.
- **Free count** (`pmm_free_count`): counts free frames using
  Brian Kernighan's bit-counting method (a standard public-domain technique).

No third-party code was used in this implementation.

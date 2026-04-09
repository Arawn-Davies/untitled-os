#include <kernel/pmm.h>
#include <kernel/tty.h>
#include <string.h>

/* 4 GB / 4 KB = 1 048 576 frames, stored as 32 768 32-bit words. */
#define PMM_MAX_FRAMES   (0x100000U)
#define PMM_BITMAP_WORDS (PMM_MAX_FRAMES / 32)

/* bit = 1 → frame used   bit = 0 → frame free */
static uint32_t bitmap[PMM_BITMAP_WORDS];

static inline void pmm_set_frame(uint32_t frame)
{
	bitmap[frame / 32] |= (1U << (frame % 32));
}

static inline void pmm_clear_frame(uint32_t frame)
{
	bitmap[frame / 32] &= ~(1U << (frame % 32));
}

void pmm_init(uint32_t magic, multiboot_info_t *mbi)
{
	extern uint32_t _kernel_end;

	/* Start with every frame marked as used. */
	memset(bitmap, 0xFF, sizeof(bitmap));

	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		t_writestring("PMM: invalid multiboot magic, no memory freed\n");
		return;
	}

	if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP)) {
		t_writestring("PMM: no memory map from bootloader, no memory freed\n");
		return;
	}

	/* Walk the memory map and mark usable regions as free. */
	multiboot_mmap_entry_t *entry =
		(multiboot_mmap_entry_t *)(uintptr_t)mbi->mmap_addr;
	multiboot_mmap_entry_t *map_end =
		(multiboot_mmap_entry_t *)(uintptr_t)(mbi->mmap_addr + mbi->mmap_length);

	while (entry < map_end) {
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
			/* Round start up and end down to 4 KiB boundaries. */
			uint64_t region_start =
				(entry->addr + PMM_FRAME_SIZE - 1) & ~(uint64_t)(PMM_FRAME_SIZE - 1);
			uint64_t region_end =
				(entry->addr + entry->len) & ~(uint64_t)(PMM_FRAME_SIZE - 1);

			for (uint64_t addr = region_start; addr < region_end; addr += PMM_FRAME_SIZE) {
				if (addr < (uint64_t)PMM_MAX_FRAMES * PMM_FRAME_SIZE)
					pmm_clear_frame((uint32_t)(addr / PMM_FRAME_SIZE));
			}
		}
		/* Advance: 'size' excludes itself, so add sizeof(size) too. */
		entry = (multiboot_mmap_entry_t *)
			((uintptr_t)entry + entry->size + sizeof(entry->size));
	}

	/* Re-mark the null page as used (guard against NULL dereferences). */
	pmm_set_frame(0);

	/* Re-mark all frames occupied by the kernel image as used.
	   The kernel is loaded at 1 MiB; _kernel_end is the first address
	   beyond the last kernel section, as defined in linker.ld. */
	uint32_t kstart = 0x100000 / PMM_FRAME_SIZE;
	uint32_t kend   = ((uint32_t)&_kernel_end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
	for (uint32_t f = kstart; f < kend; f++)
		pmm_set_frame(f);

	uint32_t free_frames = pmm_free_count();
	t_writestring("PMM: ");
	t_dec(free_frames);
	t_writestring(" frames free (");
	t_dec((free_frames * PMM_FRAME_SIZE) / (1024 * 1024));
	t_writestring(" MiB)\n");
}

uint32_t pmm_alloc_frame(void)
{
	for (uint32_t i = 0; i < PMM_BITMAP_WORDS; i++) {
		if (bitmap[i] == 0xFFFFFFFF)
			continue;  /* all 32 frames in this word are used */
		for (uint32_t bit = 0; bit < 32; bit++) {
			if (!((bitmap[i] >> bit) & 1)) {
				uint32_t frame = i * 32 + bit;
				pmm_set_frame(frame);
				return frame * PMM_FRAME_SIZE;
			}
		}
	}
	return PMM_ALLOC_ERROR;  /* out of memory */
}

void pmm_free_frame(uint32_t addr)
{
	pmm_clear_frame(addr / PMM_FRAME_SIZE);
}

uint32_t pmm_free_count(void)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < PMM_BITMAP_WORDS; i++) {
		uint32_t word = ~bitmap[i];  /* flip bits: 1 now means free */
		/* Brian Kernighan's bit-count */
		while (word) {
			count++;
			word &= word - 1;
		}
	}
	return count;
}

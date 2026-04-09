#ifndef _KERNEL_MULTIBOOT_H_
#define _KERNEL_MULTIBOOT_H_

#include <stdint.h>

/* Multiboot 2 magic value placed in eax by the bootloader. */
#define MULTIBOOT2_BOOTLOADER_MAGIC  0x36D76289

/* Multiboot 2 tag types. */
#define MULTIBOOT2_TAG_TYPE_END      0
#define MULTIBOOT2_TAG_TYPE_MMAP     6

/* Multiboot 2 memory map entry type: usable RAM. */
#define MULTIBOOT2_MEMORY_AVAILABLE  1

/* Fixed 8-byte header at the start of the Multiboot 2 information structure. */
typedef struct
{
	uint32_t total_size;
	uint32_t reserved;
} __attribute__((packed)) multiboot2_info_t;

/* Generic tag header – every tag begins with these two fields. */
typedef struct
{
	uint32_t type;
	uint32_t size;
} __attribute__((packed)) multiboot2_tag_t;

/* A single entry in the Multiboot 2 memory map. */
typedef struct
{
	uint64_t base_addr;
	uint64_t length;
	uint32_t type;
	uint32_t reserved;
} __attribute__((packed)) multiboot2_mmap_entry_t;

/* Memory-map tag (type 6). */
typedef struct
{
	uint32_t type;           /* = MULTIBOOT2_TAG_TYPE_MMAP */
	uint32_t size;
	uint32_t entry_size;
	uint32_t entry_version;
	/* multiboot2_mmap_entry_t entries[] follow immediately. */
} __attribute__((packed)) multiboot2_tag_mmap_t;

#endif

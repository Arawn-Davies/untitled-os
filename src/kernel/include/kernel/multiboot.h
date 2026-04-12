#ifndef _KERNEL_MULTIBOOT_H_
#define _KERNEL_MULTIBOOT_H_

#include <stdint.h>

/* Multiboot 2 magic value placed in eax by the bootloader. */
#define MULTIBOOT2_BOOTLOADER_MAGIC  0x36D76289

/* Multiboot 2 tag types. */
#define MULTIBOOT2_TAG_TYPE_END          0
#define MULTIBOOT2_TAG_TYPE_BOOTDEV      5
#define MULTIBOOT2_TAG_TYPE_MMAP         6
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER  8

/* Multiboot 2 memory map entry type: usable RAM. */
#define MULTIBOOT2_MEMORY_AVAILABLE  1

/* Framebuffer types reported in multiboot2_tag_framebuffer_t. */
#define MULTIBOOT2_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT2_FRAMEBUFFER_TYPE_EGA_TEXT 2

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

/* Boot device tag (type 5). */
typedef struct
{
	uint32_t type;     /* = MULTIBOOT2_TAG_TYPE_BOOTDEV */
	uint32_t size;
	uint32_t biosdev;  /* BIOS drive number (0x80 = first HDD, 0xE0 = CD-ROM) */
	uint32_t slice;    /* MBR sub-partition (0xFFFFFFFF if not applicable) */
	uint32_t part;     /* Sub-sub-partition (0xFFFFFFFF if not applicable) */
} __attribute__((packed)) multiboot2_tag_bootdev_t;

/* Memory-map tag (type 6). */
typedef struct
{
	uint32_t type;           /* = MULTIBOOT2_TAG_TYPE_MMAP */
	uint32_t size;
	uint32_t entry_size;
	uint32_t entry_version;
	/* multiboot2_mmap_entry_t entries[] follow immediately. */
} __attribute__((packed)) multiboot2_tag_mmap_t;

/* Framebuffer tag (type 8). */
typedef struct
{
	uint32_t type;               /* = MULTIBOOT2_TAG_TYPE_FRAMEBUFFER */
	uint32_t size;
	uint64_t framebuffer_addr;   /* physical base address */
	uint32_t framebuffer_pitch;  /* bytes per scanline */
	uint32_t framebuffer_width;  /* pixels per row */
	uint32_t framebuffer_height; /* rows */
	uint8_t  framebuffer_bpp;    /* bits per pixel */
	uint8_t  framebuffer_type;   /* MULTIBOOT2_FRAMEBUFFER_TYPE_* */
	uint16_t reserved;
	/* For RGB (type 1): six colour-channel descriptor bytes follow. */
	uint8_t  red_field_position;
	uint8_t  red_mask_size;
	uint8_t  green_field_position;
	uint8_t  green_mask_size;
	uint8_t  blue_field_position;
	uint8_t  blue_mask_size;
} __attribute__((packed)) multiboot2_tag_framebuffer_t;

#endif

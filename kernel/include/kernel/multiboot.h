#ifndef _KERNEL_MULTIBOOT_H_
#define _KERNEL_MULTIBOOT_H_

#include <stdint.h>

/* Multiboot 1 magic value placed in eax by the bootloader. */
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

/* Flags set in the multiboot_info flags field. */
#define MULTIBOOT_INFO_MEM_MAP      (1 << 6)

/* Memory map entry type: usable RAM. */
#define MULTIBOOT_MEMORY_AVAILABLE  1

/* Multiboot memory-map entry.
   The 'size' field is the size of the remaining fields (i.e. excludes itself). */
typedef struct
{
	uint32_t size;
	uint64_t addr;
	uint64_t len;
	uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

/* Multiboot information structure passed by the bootloader. */
typedef struct
{
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t syms[4];
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t  framebuffer_bpp;
	uint8_t  framebuffer_type;
} __attribute__((packed)) multiboot_info_t;

#endif

#ifndef _KERNEL_VESA_H
#define _KERNEL_VESA_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/multiboot.h>
#include <kernel/vesa_config.h>

/* Preferred framebuffer geometry (requested via the Multiboot 2 header tag).
 * These resolve to the values in <kernel/vesa_config.h>. */
#define VESA_PREFERRED_WIDTH   VESA_WIDTH
#define VESA_PREFERRED_HEIGHT  VESA_HEIGHT
#define VESA_PREFERRED_BPP     VESA_BPP

/* Framebuffer descriptor populated by vesa_init(). */
typedef struct
{
	uint32_t *addr;    /* mapped base address of the framebuffer */
	uint32_t  pitch;   /* bytes per scanline */
	uint32_t  width;   /* width in pixels */
	uint32_t  height;  /* height in pixels */
	uint8_t   bpp;     /* bits per pixel */
	/* RGB channel layout */
	uint8_t   red_shift;
	uint8_t   red_bits;
	uint8_t   green_shift;
	uint8_t   green_bits;
	uint8_t   blue_shift;
	uint8_t   blue_bits;
} vesa_fb_t;

/*
 * Locate the Multiboot 2 framebuffer tag and, if found, populate the global
 * framebuffer descriptor.  Returns true on success, false if no framebuffer
 * tag was found or the framebuffer type is not direct RGB.
 */
bool vesa_init(multiboot2_info_t *mbi);

/* Return a pointer to the global framebuffer descriptor, or NULL if
 * vesa_init() has not been called successfully. */
const vesa_fb_t *vesa_get_fb(void);

/* Write a 32-bit ARGB colour to pixel (x, y).
 * The caller is responsible for ensuring x < fb->width and y < fb->height;
 * out-of-bounds coordinates will corrupt adjacent memory or crash the system. */
void vesa_put_pixel(uint32_t x, uint32_t y, uint32_t colour);

/* Fill the entire framebuffer with a single 32-bit ARGB colour. */
void vesa_clear(uint32_t colour);

#endif

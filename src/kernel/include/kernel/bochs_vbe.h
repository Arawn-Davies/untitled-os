#ifndef KERNEL_BOCHS_VBE_H
#define KERNEL_BOCHS_VBE_H

#include <stdbool.h>
#include <stdint.h>

/* Bochs VBE / QEMU BGA interface — accessible via I/O ports from protected
 * mode without requiring real-mode BIOS calls. */

/* Returns true if a Bochs-compatible VBE adapter is detected. */
bool bochs_vbe_available(void);

/* Switch the linear framebuffer to width×height at bpp bits per pixel.
 * LFB_ENABLED is set so the framebuffer address stays constant.
 * Call vesa_update_geometry() + vesa_tty_init() after this. */
void bochs_vbe_set_mode(uint32_t width, uint32_t height, uint8_t bpp);

/* Disable VBE graphics.  QEMU reverts to VGA text mode (mode 3, 80×25). */
void bochs_vbe_disable(void);


#endif /* KERNEL_BOCHS_VBE_H */

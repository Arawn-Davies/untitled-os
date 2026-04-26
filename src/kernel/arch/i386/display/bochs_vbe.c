#include <kernel/bochs_vbe.h>
#include <kernel/asm.h>

#define VBE_PORT_INDEX  0x01CEu
#define VBE_PORT_DATA   0x01CFu

#define VBE_IDX_ID      0x0u
#define VBE_IDX_XRES    0x1u
#define VBE_IDX_YRES    0x2u
#define VBE_IDX_BPP     0x3u
#define VBE_IDX_ENABLE  0x4u

#define VBE_DISABLED    0x0000u
#define VBE_ENABLED     0x0001u
#define VBE_LFB         0x0040u

#define VBE_ID_MIN  0xB0C0u
#define VBE_ID_MAX  0xB0CFu

/* VGA attribute controller ports used when re-enabling text-mode display. */
#define VGA_PORT_ISR1   0x3DAu   /* input status register 1 (resets AR flip-flop) */
#define VGA_PORT_AR     0x3C0u   /* attribute controller address/data register */
#define VGA_AR_PAS      0x20u    /* Palette Address Source — bit 5 enables video */

static uint16_t vbe_read(uint16_t idx)
{
    outw(VBE_PORT_INDEX, idx);
    return inw(VBE_PORT_DATA);
}

static void vbe_write(uint16_t idx, uint16_t val)
{
    outw(VBE_PORT_INDEX, idx);
    outw(VBE_PORT_DATA, val);
}

bool bochs_vbe_available(void)
{
    uint16_t id = vbe_read(VBE_IDX_ID);
    return (id >= VBE_ID_MIN && id <= VBE_ID_MAX);
}

void bochs_vbe_set_mode(uint32_t width, uint32_t height, uint8_t bpp)
{
    vbe_write(VBE_IDX_ENABLE, VBE_DISABLED);
    vbe_write(VBE_IDX_XRES,   (uint16_t)width);
    vbe_write(VBE_IDX_YRES,   (uint16_t)height);
    vbe_write(VBE_IDX_BPP,    (uint16_t)bpp);
    vbe_write(VBE_IDX_ENABLE, VBE_ENABLED | VBE_LFB);
}

void bochs_vbe_disable(void)
{
    vbe_write(VBE_IDX_ENABLE, VBE_DISABLED);

    /* After disabling VBE the VGA attribute controller's PAS bit may be
     * clear, which blanks all video output.  Reset the flip-flop by reading
     * ISR1, then write the AR address register with PAS=1 to re-enable. */
    inb(VGA_PORT_ISR1);
    outb(VGA_PORT_AR, VGA_AR_PAS);
}

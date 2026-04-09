#include <kernel/vesa.h>
#include <kernel/tty.h>
#include <string.h>

static vesa_fb_t fb;
static bool fb_ready = false;

bool vesa_init(multiboot2_info_t *mbi)
{
	if (!mbi)
		return false;

	/* Walk the Multiboot 2 tag list looking for the framebuffer tag. */
	uint8_t *tag_ptr = (uint8_t *)mbi + sizeof(multiboot2_info_t);
	uint8_t *info_end = (uint8_t *)mbi + mbi->total_size;

	multiboot2_tag_framebuffer_t *fb_tag = NULL;

	while (tag_ptr < info_end) {
		multiboot2_tag_t *tag = (multiboot2_tag_t *)tag_ptr;

		if (tag->type == MULTIBOOT2_TAG_TYPE_END)
			break;

		if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER) {
			fb_tag = (multiboot2_tag_framebuffer_t *)tag;
			break;
		}

		tag_ptr += (tag->size + 7) & ~7u;
	}

	if (!fb_tag) {
		t_writestring("VESA: no framebuffer tag from bootloader\n");
		return false;
	}

	if (fb_tag->framebuffer_type != MULTIBOOT2_FRAMEBUFFER_TYPE_RGB) {
		t_writestring("VESA: framebuffer is not direct-colour RGB\n");
		return false;
	}

	if (fb_tag->framebuffer_bpp % 8 != 0) {
		t_writestring("VESA: framebuffer bpp is not byte-aligned\n");
		return false;
	}

	fb.addr        = (uint32_t *)(uintptr_t)fb_tag->framebuffer_addr;
	fb.pitch       = fb_tag->framebuffer_pitch;
	fb.width       = fb_tag->framebuffer_width;
	fb.height      = fb_tag->framebuffer_height;
	fb.bpp         = fb_tag->framebuffer_bpp;
	fb.red_shift   = fb_tag->red_field_position;
	fb.red_bits    = fb_tag->red_mask_size;
	fb.green_shift = fb_tag->green_field_position;
	fb.green_bits  = fb_tag->green_mask_size;
	fb.blue_shift  = fb_tag->blue_field_position;
	fb.blue_bits   = fb_tag->blue_mask_size;

	fb_ready = true;

	t_writestring("VESA: framebuffer ");
	t_dec(fb.width);
	t_writestring("x");
	t_dec(fb.height);
	t_writestring("x");
	t_dec(fb.bpp);
	t_writestring(" @ 0x");
	t_hex((uint32_t)(uintptr_t)fb.addr);
	t_writestring("\n");

	return true;
}

const vesa_fb_t *vesa_get_fb(void)
{
	return fb_ready ? &fb : NULL;
}

void vesa_put_pixel(uint32_t x, uint32_t y, uint32_t colour)
{
	if (!fb_ready)
		return;

	uint32_t bytes_per_pixel = fb.bpp / 8;
	uint8_t *pixel = (uint8_t *)fb.addr + y * fb.pitch + x * bytes_per_pixel;

	/* Write only as many bytes as the pixel format requires. */
	for (uint32_t i = 0; i < bytes_per_pixel && i < 4; i++)
		pixel[i] = (uint8_t)(colour >> (i * 8));
}

void vesa_clear(uint32_t colour)
{
	if (!fb_ready)
		return;

	for (uint32_t y = 0; y < fb.height; y++)
		for (uint32_t x = 0; x < fb.width; x++)
			vesa_put_pixel(x, y, colour);
}

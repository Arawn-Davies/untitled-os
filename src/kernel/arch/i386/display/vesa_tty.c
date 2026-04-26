#include <kernel/vesa_tty.h>
#include <kernel/vesa.h>
#include <kernel/vesa_font.h>
#include <kernel/paging.h>
#include <kernel/serial.h>
#include <string.h>

/* Scale factor applied to every glyph pixel.  2 makes each 8×8 glyph appear
   as a 16×16 cell on the framebuffer, which is far more readable at typical
   VESA resolutions.  Can be changed at runtime via vesa_tty_set_scale(). */
static uint32_t font_scale = 2;

#define FONT_CELL_W (FONT8x8_CHAR_W * font_scale)
#define FONT_CELL_H (FONT8x8_CHAR_H * font_scale)

static bool     tty_ready = false;
static uint32_t tty_fg;        /* foreground colour in framebuffer format */
static uint32_t tty_bg;        /* background colour in framebuffer format */
static uint32_t tty_col;       /* cursor column (in characters) */
static uint32_t tty_row;       /* cursor row    (in characters) */
static uint32_t tty_cols;      /* total columns on screen */
static uint32_t tty_rows;      /* total rows on screen */

/* Compose a framebuffer pixel value from 8-bit R, G, B components using the
   channel layout reported by the bootloader. */
static uint32_t compose_colour(uint8_t r, uint8_t g, uint8_t b)
{
	const vesa_fb_t *fb = vesa_get_fb();
	return ((uint32_t)r << fb->red_shift)   |
	       ((uint32_t)g << fb->green_shift) |
	       ((uint32_t)b << fb->blue_shift);
}

bool vesa_tty_init(void)
{
	const vesa_fb_t *fb = vesa_get_fb();
	if (!fb)
		return false;

	/* The framebuffer lives at a high physical address outside the boot-time
	   0–8 MiB identity mapping.  Map it before writing a single pixel. */
	paging_map_region((uint32_t)(uintptr_t)fb->addr,
	                  fb->pitch * fb->height);

	tty_cols = fb->width  / FONT_CELL_W;
	tty_rows = fb->height / FONT_CELL_H;
	tty_col  = 0;
	tty_row  = 0;

	tty_fg = compose_colour(0xFF, 0xFF, 0xFF); /* white */
	tty_bg = compose_colour(0x00, 0x00, 0x00); /* black */

	vesa_clear(tty_bg);

	tty_ready = true;
	KLOG("vesa_tty: init OK (");
	KLOG_DEC(tty_cols);
	KLOG("x");
	KLOG_DEC(tty_rows);
	KLOG(" chars, scale=");
	KLOG_DEC(font_scale);
	KLOG(")\n");
	return true;
}

bool vesa_tty_is_ready(void)
{
	return tty_ready;
}

uint32_t vesa_tty_get_row(void)
{
	return tty_row;
}

uint32_t vesa_tty_get_cols(void)
{
	return tty_cols;
}

uint32_t vesa_tty_get_rows(void)
{
	return tty_rows;
}

void vesa_tty_disable(void)
{
	tty_ready = false;
}

void vesa_tty_setcolor(uint32_t fg_rgb, uint32_t bg_rgb)
{
	tty_fg = compose_colour(
		(uint8_t)((fg_rgb >> 16) & 0xFF),
		(uint8_t)((fg_rgb >>  8) & 0xFF),
		(uint8_t)( fg_rgb        & 0xFF));
	tty_bg = compose_colour(
		(uint8_t)((bg_rgb >> 16) & 0xFF),
		(uint8_t)((bg_rgb >>  8) & 0xFF),
		(uint8_t)( bg_rgb        & 0xFF));
}

/* Render glyph for character c at cell (col, row). */
static void draw_char(char c, uint32_t col, uint32_t row)
{
	uint8_t idx = (uint8_t)c;
	if (idx >= 128)
		idx = 0;

	const uint8_t *glyph = FONT8x8[idx];
	uint32_t px = col * FONT_CELL_W;
	uint32_t py = row * FONT_CELL_H;

	for (uint32_t y = 0; y < FONT8x8_CHAR_H; y++) {
		uint8_t bits = glyph[y];
		for (uint32_t x = 0; x < FONT8x8_CHAR_W; x++) {
			uint32_t colour = (bits & (1u << x)) ? tty_fg : tty_bg;
			/* Each source pixel becomes a font_scale × font_scale block. */
			for (uint32_t sy = 0; sy < font_scale; sy++)
				for (uint32_t sx = 0; sx < font_scale; sx++)
					vesa_put_pixel(px + x * font_scale + sx,
					               py + y * font_scale + sy, colour);
		}
	}
}

/* Scroll the text area up by one character row. */
static void scroll_up(void)
{
	const vesa_fb_t *fb = vesa_get_fb();
	uint8_t *base = (uint8_t *)fb->addr;
	uint32_t row_bytes = fb->width * (fb->bpp / 8);

	/* Copy all rows up by FONT_CELL_H scanlines. */
	for (uint32_t y = 0; y < (tty_rows - 1) * FONT_CELL_H; y++) {
		uint8_t *dst = base + y * fb->pitch;
		const uint8_t *src = base + (y + FONT_CELL_H) * fb->pitch;
		memcpy(dst, src, row_bytes);
	}

	/* Clear the newly vacated bottom character row. */
	for (uint32_t y = (tty_rows - 1) * FONT_CELL_H;
	     y < tty_rows * FONT_CELL_H; y++) {
		for (uint32_t x = 0; x < fb->width; x++)
			vesa_put_pixel(x, y, tty_bg);
	}
}

void vesa_tty_putchar(char c)
{
	if (!tty_ready)
		return;

	if (c == '\n') {
		tty_col = 0;
		if (++tty_row >= tty_rows) {
			scroll_up();
			tty_row = tty_rows - 1;
		}
		return;
	}

	if (c == '\r') {
		tty_col = 0;
		return;
	}

	if (c == '\b') {
		if (tty_col > 0)
			tty_col--;
		draw_char(' ', tty_col, tty_row);
		return;
	}

	draw_char(c, tty_col, tty_row);

	if (++tty_col >= tty_cols) {
		tty_col = 0;
		if (++tty_row >= tty_rows) {
			scroll_up();
			tty_row = tty_rows - 1;
		}
	}
}

void vesa_tty_put_at(char c, uint32_t col, uint32_t row)
{
	if (!tty_ready)
		return;
	if (col >= tty_cols || row >= tty_rows)
		return;
	draw_char(c, col, row);
}

void vesa_tty_spinner_tick(uint32_t tick)
{
	if (!tty_ready)
		return;
	static const char frames[] = {'|', '/', '-', '\\'};
	char c = frames[(tick / 12) % 4];
	vesa_tty_put_at(c, tty_cols - 1, 0);
}

void vesa_tty_set_scale(uint32_t scale)
{
	if (scale == 0)
		scale = 1;
	font_scale = scale;   /* always update — vesa_tty_init() uses this */
}

void vesa_tty_clear(void)
{
	if (!tty_ready)
		return;
	tty_col = 0;
	tty_row = 0;
	vesa_clear(tty_bg);
}

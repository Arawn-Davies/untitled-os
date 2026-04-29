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

static bool         tty_ready = false;
static uint32_t     tty_cols;          /* total columns on screen */
static uint32_t     tty_rows;          /* total rows on screen    */
static vesa_pane_t  default_pane;      /* covers the whole screen */

/* Compose a framebuffer pixel value from 8-bit R, G, B components using the
   channel layout reported by the bootloader. */
static uint32_t compose_colour(uint8_t r, uint8_t g, uint8_t b)
{
	const vesa_fb_t *fb = vesa_get_fb();
	return ((uint32_t)r << fb->red_shift)   |
	       ((uint32_t)g << fb->green_shift) |
	       ((uint32_t)b << fb->blue_shift);
}

static uint32_t compose_rgb(uint32_t rgb)
{
	return compose_colour((uint8_t)((rgb >> 16) & 0xFF),
	                      (uint8_t)((rgb >>  8) & 0xFF),
	                      (uint8_t)( rgb        & 0xFF));
}

/* Render glyph for character c at absolute cell (abs_col, abs_row) using the
 * given pane's fg/bg.  Caller has already clipped to the pane. */
static void draw_char(const vesa_pane_t *p, char c,
                      uint32_t abs_col, uint32_t abs_row)
{
	uint8_t idx = (uint8_t)c;
	if (idx >= 128)
		idx = 0;

	const uint8_t *glyph = FONT8x8[idx];
	uint32_t px = abs_col * FONT_CELL_W;
	uint32_t py = abs_row * FONT_CELL_H;

	for (uint32_t y = 0; y < FONT8x8_CHAR_H; y++) {
		uint8_t bits = glyph[y];
		for (uint32_t x = 0; x < FONT8x8_CHAR_W; x++) {
			uint32_t colour = (bits & (1u << x)) ? p->fg : p->bg;
			for (uint32_t sy = 0; sy < font_scale; sy++)
				for (uint32_t sx = 0; sx < font_scale; sx++)
					vesa_put_pixel(px + x * font_scale + sx,
					               py + y * font_scale + sy, colour);
		}
	}
}

/* Scroll a pane up by one character row, clearing the bottom row to bg. */
static void pane_scroll_up(vesa_pane_t *p)
{
	const vesa_fb_t *fb = vesa_get_fb();
	uint8_t *base = (uint8_t *)fb->addr;

	uint32_t y_top    = p->top_row * FONT_CELL_H;
	uint32_t y_bottom = (p->top_row + p->rows) * FONT_CELL_H; /* exclusive */
	uint32_t shift    = FONT_CELL_H;

	if (p->rows > 1) {
		memmove(base + y_top * fb->pitch,
		        base + (y_top + shift) * fb->pitch,
		        (p->rows - 1) * FONT_CELL_H * fb->pitch);
	}

	/* Clear the newly vacated bottom row of the pane. */
	for (uint32_t y = y_bottom - shift; y < y_bottom; y++) {
		uint32_t *scanline = (uint32_t *)((uint8_t *)fb->addr + y * fb->pitch);
		for (uint32_t x = 0; x < fb->width; x++)
			scanline[x] = p->bg;
	}
}

/* ------------------------------------------------------------------ */
/* Init / status                                                       */
/* ------------------------------------------------------------------ */

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

	default_pane.top_row = 0;
	default_pane.cols    = tty_cols;
	default_pane.rows    = tty_rows;
	default_pane.cur_col = 0;
	default_pane.cur_row = 0;
	default_pane.fg = compose_colour(0xFF, 0xFF, 0xFF); /* white */
	default_pane.bg = compose_colour(0x00, 0x00, 0xAA); /* blue  */

	vesa_clear(default_pane.bg);

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

bool vesa_tty_is_ready(void) { return tty_ready; }
void vesa_tty_disable(void)  { tty_ready = false; }

uint32_t vesa_tty_get_cols(void) { return tty_cols; }
uint32_t vesa_tty_get_rows(void) { return tty_rows; }

vesa_pane_t *vesa_tty_default_pane(void) { return &default_pane; }

/* ------------------------------------------------------------------ */
/* Pane API                                                            */
/* ------------------------------------------------------------------ */

void vesa_tty_pane_init(vesa_pane_t *p, uint32_t top_row, uint32_t rows)
{
	p->top_row = top_row;
	p->cols    = tty_cols;     /* zero before init — fine, gets fixed up later */
	p->rows    = rows;
	p->cur_col = 0;
	p->cur_row = 0;
	/* Default colours; caller can override before drawing. */
	if (tty_ready) {
		p->fg = compose_colour(0xFF, 0xFF, 0xFF);
		p->bg = compose_colour(0x00, 0x00, 0x00);
	} else {
		p->fg = 0;
		p->bg = 0;
	}
}

void vesa_tty_pane_setcolor(vesa_pane_t *p, uint32_t fg_rgb, uint32_t bg_rgb)
{
	p->fg = compose_rgb(fg_rgb);
	p->bg = compose_rgb(bg_rgb);
}

void vesa_tty_pane_putchar(vesa_pane_t *p, char c)
{
	if (!tty_ready)
		return;

	/* The timer ISR may preempt between a cursor increment and its bounds
	 * check, leaving cur_col/cur_row transiently out of range.  Clamp here
	 * so the draw never receives an invalid position. */
	if (p->cur_col >= p->cols && p->cols > 0) {
		p->cur_col = 0;
		if (++p->cur_row >= p->rows) {
			pane_scroll_up(p);
			p->cur_row = p->rows - 1;
		}
	}
	if (p->cur_row >= p->rows && p->rows > 0) {
		pane_scroll_up(p);
		p->cur_row = p->rows - 1;
	}

	if (c == '\n') {
		p->cur_col = 0;
		if (++p->cur_row >= p->rows) {
			pane_scroll_up(p);
			p->cur_row = p->rows - 1;
		}
		return;
	}

	if (c == '\r') {
		p->cur_col = 0;
		return;
	}

	if (c == '\b') {
		if (p->cur_col > 0)
			p->cur_col--;
		draw_char(p, ' ', p->cur_col, p->top_row + p->cur_row);
		return;
	}

	draw_char(p, c, p->cur_col, p->top_row + p->cur_row);

	if (++p->cur_col >= p->cols) {
		p->cur_col = 0;
		if (++p->cur_row >= p->rows) {
			pane_scroll_up(p);
			p->cur_row = p->rows - 1;
		}
	}
}

void vesa_tty_pane_put_at(vesa_pane_t *p, char c, uint32_t col, uint32_t row)
{
	if (!tty_ready)
		return;
	if (col >= p->cols || row >= p->rows)
		return;
	draw_char(p, c, col, p->top_row + row);
}

void vesa_tty_pane_set_cursor(vesa_pane_t *p, uint32_t col, uint32_t row)
{
	if (col < p->cols) p->cur_col = col;
	if (row < p->rows) p->cur_row = row;
}

void vesa_tty_pane_clear(vesa_pane_t *p)
{
	p->cur_col = 0;
	p->cur_row = 0;
	if (!tty_ready)
		return;

	const vesa_fb_t *fb = vesa_get_fb();
	uint32_t y_top    = p->top_row * FONT_CELL_H;
	uint32_t y_bottom = (p->top_row + p->rows) * FONT_CELL_H;
	for (uint32_t y = y_top; y < y_bottom; y++) {
		uint32_t *scanline = (uint32_t *)((uint8_t *)fb->addr + y * fb->pitch);
		for (uint32_t x = 0; x < fb->width; x++)
			scanline[x] = p->bg;
	}
}

uint32_t vesa_tty_pane_get_col(const vesa_pane_t *p) { return p->cur_col; }
uint32_t vesa_tty_pane_get_row(const vesa_pane_t *p) { return p->cur_row; }

/* ------------------------------------------------------------------ */
/* Legacy global API — delegates to the default pane                  */
/* ------------------------------------------------------------------ */

uint32_t vesa_tty_get_col(void) { return default_pane.cur_col; }
uint32_t vesa_tty_get_row(void) { return default_pane.cur_row; }

void vesa_tty_putchar(char c)
{
	vesa_tty_pane_putchar(&default_pane, c);
}

void vesa_tty_setcolor(uint32_t fg_rgb, uint32_t bg_rgb)
{
	vesa_tty_pane_setcolor(&default_pane, fg_rgb, bg_rgb);
}

void vesa_tty_put_at(char c, uint32_t col, uint32_t row)
{
	vesa_tty_pane_put_at(&default_pane, c, col, row);
}

void vesa_tty_set_cursor(uint32_t col, uint32_t row)
{
	vesa_tty_pane_set_cursor(&default_pane, col, row);
}

void vesa_tty_clear(void)
{
	if (!tty_ready)
		return;
	/* Match legacy behaviour: clear the whole screen, including any sub-pane
	 * regions that may have been carved out by phase-3 callers. */
	vesa_clear(default_pane.bg);
	default_pane.cur_col = 0;
	default_pane.cur_row = 0;
}

void vesa_tty_spinner_tick(uint32_t tick)
{
	if (!tty_ready)
		return;
	static const char frames[] = {'|', '/', '-', '\\'};
	char c = frames[(tick / 12) % 4];
	/* Always top-right of the physical screen, regardless of pane carve-up. */
	draw_char(&default_pane, c, tty_cols - 1, 0);
}

void vesa_tty_set_scale(uint32_t scale)
{
	if (scale == 0)
		scale = 1;
	font_scale = scale;   /* always update — vesa_tty_init() uses this */
}

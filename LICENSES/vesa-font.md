# VESA 8×8 Bitmap Font

**File:** `kernel/include/kernel/vesa_font.h`

## License / Origin

The `FONT8x8` glyph table is based on the classic IBM PC VGA 8×8 character ROM
glyph set. This glyph set is widely reproduced across open-source operating
system projects and is generally considered to be in the **public domain**.

No copyright claim is made over the glyph data included in this project.

## Description

`FONT8x8` is a 128-entry table covering ASCII code points 0x00–0x7F.  Each
entry is 8 bytes (one byte per scanline, 8 scanlines tall).  Within each byte,
bit 0 is the leftmost pixel and bit 7 is the rightmost pixel.

The font is used by the VESA framebuffer text renderer
(`kernel/arch/i386/vesa_tty.c`) to draw characters directly into the linear
framebuffer when a graphical VESA/BIOS mode is active.

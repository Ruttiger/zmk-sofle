/*
 * Portrait 64x128 primitives mapped directly into the physical SSD1306
 * vertical-tile framebuffer. There is deliberately no portrait framebuffer.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "oled_font_5x7.h"
#include "oled_gfx.h"
#include "oled_raw_ssd1306_128x64.h"

#define PORTRAIT_WIDTH 64
#define PORTRAIT_HEIGHT 128
#define PHYSICAL_WIDTH 128
#define PHYSICAL_HEIGHT 64

static uint8_t *oled_gfx_framebuffer;
static const struct display_capabilities *oled_gfx_caps;

void oled_gfx_init(uint8_t *physical_framebuffer,
		   const struct display_capabilities *capabilities)
{
	oled_gfx_framebuffer = physical_framebuffer;
	oled_gfx_caps = capabilities;
}

void oled_gfx_clear(void)
{
	if (oled_gfx_framebuffer != NULL) {
		/* With SSD1306 inversion-on, 1 is the physical black/background bit. */
		memset(oled_gfx_framebuffer, 0xff, OLED_RAW_BUFFER_SIZE);
	}
}

void oled_gfx_set_pixel(int x, int y, bool on)
{
	int physical_x;
	int physical_y;
	size_t index;
	uint8_t bit;

	if (oled_gfx_framebuffer == NULL || oled_gfx_caps == NULL ||
	    x < 0 || x >= PORTRAIT_WIDTH || y < 0 || y >= PORTRAIT_HEIGHT) {
		return;
	}

	/* Calibration transform: physical_x = logical_y; physical_y = 63 - logical_x. */
	physical_x = y;
	physical_y = (int)(PHYSICAL_HEIGHT - 1U) - x;
	if (physical_x < 0 || physical_x >= (int)PHYSICAL_WIDTH ||
	    physical_y < 0 || physical_y >= (int)PHYSICAL_HEIGHT) {
		return;
	}

	index = ((size_t)physical_y / 8U) * OLED_RAW_WIDTH +
		(size_t)physical_x;
	bit = (oled_gfx_caps->screen_info & SCREEN_INFO_MONO_MSB_FIRST) ?
		(7U - (physical_y & 7)) : (physical_y & 7);

	if (on) {
		/* MONO10 + inversion-on: zero is the visible white pixel. */
		oled_gfx_framebuffer[index] &= (uint8_t)~BIT(bit);
	} else {
		oled_gfx_framebuffer[index] |= BIT(bit);
	}
}

void oled_gfx_hline(int x0, int x1, int y, bool on)
{
	int x;

	if (x0 > x1) {
		int tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	for (x = x0; x <= x1; ++x) {
		oled_gfx_set_pixel(x, y, on);
	}
}

void oled_gfx_vline(int x, int y0, int y1, bool on)
{
	int y;

	if (y0 > y1) {
		int tmp = y0;
		y0 = y1;
		y1 = tmp;
	}

	for (y = y0; y <= y1; ++y) {
		oled_gfx_set_pixel(x, y, on);
	}
}

void oled_gfx_rect(int x, int y, int width, int height, bool on)
{
	if (width <= 0 || height <= 0) {
		return;
	}

	oled_gfx_hline(x, x + width - 1, y, on);
	oled_gfx_hline(x, x + width - 1, y + height - 1, on);
	oled_gfx_vline(x, y, y + height - 1, on);
	oled_gfx_vline(x + width - 1, y, y + height - 1, on);
}

void oled_gfx_fill_rect(int x, int y, int width, int height, bool on)
{
	int row;

	if (width <= 0 || height <= 0) {
		return;
	}

	for (row = y; row < y + height; ++row) {
		oled_gfx_hline(x, x + width - 1, row, on);
	}
}

void oled_gfx_draw_char(int x, int y, char ch, bool on)
{
	const uint8_t *glyph = oled_font_5x7_glyph(ch);

	for (int row = 0; row < OLED_FONT_5X7_HEIGHT; ++row) {
		for (int col = 0; col < OLED_FONT_5X7_WIDTH; ++col) {
			if (glyph[row] & BIT(OLED_FONT_5X7_WIDTH - 1 - col)) {
				oled_gfx_set_pixel(x + col, y + row, on);
			}
		}
	}
}

void oled_gfx_draw_text(int x, int y, const char *text, bool on)
{
	if (text == NULL) {
		return;
	}

	while (*text != '\0') {
		oled_gfx_draw_char(x, y, *text, on);
		x += OLED_FONT_5X7_WIDTH + 1;
		++text;
	}
}

int oled_gfx_flush(void)
{
	return oled_raw_flush();
}

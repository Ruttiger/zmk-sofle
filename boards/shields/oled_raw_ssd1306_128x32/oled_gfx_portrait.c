/*
 * Fixed 32x128 portrait primitives mapped directly into the physical
 * SSD1306 vertical-tile framebuffer. There is no secondary canvas.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "oled_gfx_portrait.h"
#include "oled_raw_ssd1306_128x32.h"

#define PHYSICAL_WIDTH 128U
#define PHYSICAL_HEIGHT 32U
#define BUFFER_SIZE 512U

static uint8_t *oled_gfx_framebuffer;
static const struct display_capabilities *oled_gfx_caps;

void oled_gfx_init(void)
{
	oled_gfx_framebuffer = oled_raw_ssd1306_128x32_framebuffer();
	oled_gfx_caps = oled_raw_ssd1306_128x32_capabilities();
}

void oled_gfx_set_pixel(int x, int y, bool on)
{
	int physical_x;
	int physical_y;
	size_t index;
	uint8_t bit;

	if (oled_gfx_framebuffer == NULL || oled_gfx_caps == NULL ||
	    x < 0 || x >= OLED_GFX_WIDTH ||
	    y < 0 || y >= OLED_GFX_HEIGHT) {
		return;
	}

	/* Frozen mapping: physical_x = 127 - logical_y; physical_y = logical_x. */
	physical_x = 127 - y;
	physical_y = x;

	index = ((size_t)physical_y / 8U) * PHYSICAL_WIDTH +
		(size_t)physical_x;
	bit = (oled_gfx_caps->screen_info & SCREEN_INFO_MONO_MSB_FIRST) ?
		(7U - (physical_y & 7U)) : (physical_y & 7U);

	/* MONO10 + inversion-on: zero is a visible pixel. */
	if (on) {
		oled_gfx_framebuffer[index] &= (uint8_t)~BIT(bit);
	} else {
		oled_gfx_framebuffer[index] |= BIT(bit);
	}
}

void oled_gfx_hline(int x0, int x1, int y, bool on)
{
	if (x0 > x1) {
		int tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	for (int x = x0; x <= x1; ++x) {
		oled_gfx_set_pixel(x, y, on);
	}
}

void oled_gfx_vline(int x, int y0, int y1, bool on)
{
	if (y0 > y1) {
		int tmp = y0;
		y0 = y1;
		y1 = tmp;
	}

	for (int y = y0; y <= y1; ++y) {
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
	if (width <= 0 || height <= 0) {
		return;
	}

	for (int row = y; row < y + height; ++row) {
		oled_gfx_hline(x, x + width - 1, row, on);
	}
}

void oled_gfx_clear(void)
{
	if (oled_gfx_framebuffer != NULL) {
		memset(oled_gfx_framebuffer, 0xff, BUFFER_SIZE);
	}
}

int oled_gfx_flush(void)
{
	return oled_raw_ssd1306_128x32_flush();
}

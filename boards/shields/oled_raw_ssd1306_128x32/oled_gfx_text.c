/*
 * 5x7 text primitives for the fixed portrait graphics layer.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>

#include <zephyr/sys/util.h>

#include "oled_font_5x7.h"
#include "oled_gfx_portrait.h"

void oled_gfx_draw_char(int x, int y, char ch, bool on)
{
	const uint8_t *glyph = oled_font_5x7_glyph(ch);

	for (int row = 0; row < OLED_FONT_5X7_HEIGHT; ++row) {
		for (int col = 0; col < OLED_FONT_5X7_WIDTH; ++col) {
			/* Font rows define bit 4 as left and bit 0 as right. */
			if (glyph[row] & BIT(4 - col)) {
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

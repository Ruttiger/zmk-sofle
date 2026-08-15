/*
 * Compact 5x7 monochrome bitmap font.
 * Each glyph is seven row bytes; bit 0 is the leftmost pixel.
 * Lowercase letters intentionally reuse their uppercase glyphs.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OLED_FONT_5X7_H_
#define OLED_FONT_5X7_H_

#include <stdint.h>

#define OLED_FONT_5X7_WIDTH 5
#define OLED_FONT_5X7_HEIGHT 7

const uint8_t *oled_font_5x7_glyph(char ch);

#endif /* OLED_FONT_5X7_H_ */

/*
 * Monochrome 5x7 bitmap font.
 *
 * Each glyph contains seven row bytes. In every row, bit 4 is the leftmost
 * pixel and bit 0 is the rightmost pixel.
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

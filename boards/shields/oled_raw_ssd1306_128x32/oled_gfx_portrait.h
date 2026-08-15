/*
 * Fixed 32x128 portrait graphics over the physical SSD1306 framebuffer.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OLED_GFX_PORTRAIT_H_
#define OLED_GFX_PORTRAIT_H_

#include <stdbool.h>

#define OLED_GFX_WIDTH 32
#define OLED_GFX_HEIGHT 128

void oled_gfx_init(void);
void oled_gfx_set_pixel(int x, int y, bool on);
void oled_gfx_hline(int x0, int x1, int y, bool on);
void oled_gfx_vline(int x, int y0, int y1, bool on);
void oled_gfx_rect(int x, int y, int width, int height, bool on);
void oled_gfx_fill_rect(int x, int y, int width, int height, bool on);
void oled_gfx_draw_char(int x, int y, char ch, bool on);
void oled_gfx_draw_text(int x, int y, const char *text, bool on);
void oled_gfx_clear(void);
int oled_gfx_flush(void);

#endif /* OLED_GFX_PORTRAIT_H_ */

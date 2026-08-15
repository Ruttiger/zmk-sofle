/*
 * Raw SSD1306 128x32 production backend.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OLED_RAW_SSD1306_128X32_H_
#define OLED_RAW_SSD1306_128X32_H_

#include <stdint.h>

#include <zephyr/drivers/display.h>

uint8_t *oled_raw_ssd1306_128x32_framebuffer(void);
const struct display_capabilities *
oled_raw_ssd1306_128x32_capabilities(void);
int oled_raw_ssd1306_128x32_flush(void);

#endif /* OLED_RAW_SSD1306_128X32_H_ */

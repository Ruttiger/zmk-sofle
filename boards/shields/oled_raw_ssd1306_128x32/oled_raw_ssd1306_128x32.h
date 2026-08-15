/*
 * Raw SSD1306 128x32 production backend.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OLED_RAW_SSD1306_128X32_H_
#define OLED_RAW_SSD1306_128X32_H_

#include <stdint.h>

#include <zephyr/drivers/display.h>

#define OLED_RAW_WIDTH 128U
#define OLED_RAW_HEIGHT 32U
#define OLED_RAW_BUFFER_SIZE 512U

int oled_raw_prepare(void);
uint8_t *oled_raw_framebuffer(void);
const struct display_capabilities *oled_raw_capabilities(void);
int oled_raw_flush(void);

#endif /* OLED_RAW_SSD1306_128X32_H_ */

/*
 * Low-level RAW SSD1306 transport for the UI V1 shield.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OLED_RAW_SSD1306_128X64_H_
#define OLED_RAW_SSD1306_128X64_H_

#include <stdint.h>

#include <zephyr/drivers/display.h>

#define OLED_RAW_WIDTH 128U
#define OLED_RAW_HEIGHT 64U
#define OLED_RAW_BUFFER_SIZE (OLED_RAW_WIDTH * (OLED_RAW_HEIGHT / 8U))

/* Deferred device/capability validation plus the initial physical clear. */
int oled_raw_prepare(void);

uint8_t *oled_raw_framebuffer(void);
const struct display_capabilities *oled_raw_capabilities(void);

int oled_raw_flush(void);

#endif /* OLED_RAW_SSD1306_128X64_H_ */

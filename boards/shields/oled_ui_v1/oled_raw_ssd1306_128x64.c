/*
 * Low-level RAW SSD1306 128x64 backend.
 *
 * This intentionally keeps the validated display contract: one physical
 * 1024-byte framebuffer, MONO10, vertical tiling, and the normal Zephyr
 * display_write()/display_blanking_off() path. Pixel bit order follows the
 * SCREEN_INFO_MONO_MSB_FIRST flag reported by the display.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include "oled_raw_ssd1306_128x64.h"

LOG_MODULE_REGISTER(oled_ui_v1_raw, CONFIG_DISPLAY_LOG_LEVEL);

static const struct device *const oled_raw_display =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static uint8_t oled_raw_framebuffer_storage[OLED_RAW_BUFFER_SIZE];
static uint8_t oled_raw_clear_buffer[OLED_RAW_BUFFER_SIZE];
static struct display_capabilities oled_raw_caps;
static bool oled_raw_prepared;

static const struct display_buffer_descriptor oled_raw_buffer_descriptor = {
	.buf_size = sizeof(oled_raw_framebuffer_storage),
	.width = OLED_RAW_WIDTH,
	.height = OLED_RAW_HEIGHT,
	.pitch = OLED_RAW_WIDTH,
};

int oled_raw_prepare(void)
{
	int ret;

	if (oled_raw_prepared) {
		return 0;
	}

	if (!device_is_ready(oled_raw_display)) {
		LOG_ERR("display device %s is not ready; UI V1 aborted",
			oled_raw_display->name);
		return -ENODEV;
	}

	display_get_capabilities(oled_raw_display, &oled_raw_caps);
	LOG_INF("capabilities x=%u y=%u supported=0x%08x current=0x%x "
		"screen_info=0x%08x VTILED=%s MSB_FIRST=%s",
		oled_raw_caps.x_resolution, oled_raw_caps.y_resolution,
		oled_raw_caps.supported_pixel_formats,
		oled_raw_caps.current_pixel_format, oled_raw_caps.screen_info,
		(oled_raw_caps.screen_info & SCREEN_INFO_MONO_VTILED) ? "yes" : "no",
		(oled_raw_caps.screen_info & SCREEN_INFO_MONO_MSB_FIRST) ? "yes" : "no");

	if (oled_raw_caps.x_resolution != OLED_RAW_WIDTH ||
	    oled_raw_caps.y_resolution != OLED_RAW_HEIGHT ||
	    !(oled_raw_caps.supported_pixel_formats & PIXEL_FORMAT_MONO10) ||
	    oled_raw_caps.current_pixel_format != PIXEL_FORMAT_MONO10 ||
	    !(oled_raw_caps.screen_info & SCREEN_INFO_MONO_VTILED)) {
		LOG_ERR("unsupported SSD1306 capabilities; UI V1 aborted");
		return -ENOTSUP;
	}

	memset(oled_raw_clear_buffer, 0, sizeof(oled_raw_clear_buffer));
	ret = display_write(oled_raw_display, 0, 0, &oled_raw_buffer_descriptor,
				    oled_raw_clear_buffer);
	if (ret < 0) {
		LOG_ERR("initial display clear failed: %d", ret);
		return ret;
	}

	oled_raw_prepared = true;
	return 0;
}

uint8_t *oled_raw_framebuffer(void)
{
	return oled_raw_framebuffer_storage;
}

const struct display_capabilities *oled_raw_capabilities(void)
{
	return &oled_raw_caps;
}

int oled_raw_flush(void)
{
	int ret;

	ret = display_write(oled_raw_display, 0, 0, &oled_raw_buffer_descriptor,
				   oled_raw_framebuffer_storage);
	if (ret < 0) {
		LOG_ERR("display_write failed: %d", ret);
		return ret;
	}

	ret = display_blanking_off(oled_raw_display);
	if (ret < 0) {
		LOG_ERR("display_blanking_off failed: %d", ret);
	}

	return ret;
}

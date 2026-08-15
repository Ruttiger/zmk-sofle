/*
 * Raw SSD1306 128x32 production backend.
 *
 * This is the physically validated LEFT backend. It owns the only 512-byte
 * framebuffer and deliberately has no LVGL, nice_oled, or secondary canvas.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "oled_raw_ssd1306_128x32.h"

LOG_MODULE_REGISTER(oled_raw_ssd1306_128x32, CONFIG_DISPLAY_LOG_LEVEL);

BUILD_ASSERT(OLED_RAW_BUFFER_SIZE ==
	     (OLED_RAW_WIDTH * OLED_RAW_HEIGHT / 8U),
	     "SSD1306 128x32 framebuffer must be exactly 512 bytes");

static const char oled_raw_build_marker[] =
	"RUTTIGER_RAW_SSD1306_128X32_PRODUCTION_BASELINE_V1";
static const struct device *const oled_raw_display =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static uint8_t oled_raw_framebuffer_storage[OLED_RAW_BUFFER_SIZE];
static struct display_capabilities oled_raw_caps;
static bool oled_raw_ready;

static const struct display_buffer_descriptor oled_raw_buffer_descriptor = {
	.buf_size = sizeof(oled_raw_framebuffer_storage),
	.width = OLED_RAW_WIDTH,
	.height = OLED_RAW_HEIGHT,
	.pitch = OLED_RAW_WIDTH,
};

uint8_t *oled_raw_framebuffer(void)
{
	return oled_raw_framebuffer_storage;
}

const struct display_capabilities *oled_raw_capabilities(void)
{
	return oled_raw_ready ? &oled_raw_caps : NULL;
}

int oled_raw_flush(void)
{
	int ret;

	if (!oled_raw_ready) {
		return -ENODEV;
	}

	ret = display_write(oled_raw_display, 0, 0,
			    &oled_raw_buffer_descriptor,
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

static bool oled_raw_capabilities_supported(void)
{
	return oled_raw_caps.x_resolution == OLED_RAW_WIDTH &&
	       oled_raw_caps.y_resolution == OLED_RAW_HEIGHT &&
	       (oled_raw_caps.supported_pixel_formats & PIXEL_FORMAT_MONO10) &&
	       oled_raw_caps.current_pixel_format == PIXEL_FORMAT_MONO10 &&
	       (oled_raw_caps.screen_info & SCREEN_INFO_MONO_VTILED);
}

/* Zephyr v0.3 has no public display_clear() operation. Reuse the one buffer. */
static int oled_raw_clear_display(void)
{
	memset(oled_raw_framebuffer_storage, 0,
	       sizeof(oled_raw_framebuffer_storage));

	return display_write(oled_raw_display, 0, 0,
			     &oled_raw_buffer_descriptor,
			     oled_raw_framebuffer_storage);
}

int oled_raw_prepare(void)
{
	int ret;

	if (oled_raw_ready) {
		return 0;
	}

	LOG_INF("build marker: %s", oled_raw_build_marker);

	if (!device_is_ready(oled_raw_display)) {
		LOG_ERR("display device %s is not ready; OLED skipped",
			oled_raw_display->name);
		return -ENODEV;
	}

	display_get_capabilities(oled_raw_display, &oled_raw_caps);
	LOG_INF("capabilities x=%u y=%u supported=0x%08x current=0x%x "
		 "screen_info=0x%08x VTILED=%s MSB_FIRST=%s",
		oled_raw_caps.x_resolution, oled_raw_caps.y_resolution,
		oled_raw_caps.supported_pixel_formats,
		oled_raw_caps.current_pixel_format,
		oled_raw_caps.screen_info,
		(oled_raw_caps.screen_info & SCREEN_INFO_MONO_VTILED) ?
			"yes" : "no",
		(oled_raw_caps.screen_info & SCREEN_INFO_MONO_MSB_FIRST) ?
			"yes" : "no");

	if (!oled_raw_capabilities_supported()) {
		LOG_ERR("unsupported display capabilities; expected 128x32 "
			"MONO10 MONO_VTILED");
		return -ENOTSUP;
	}

	oled_raw_ready = true;
	ret = oled_raw_clear_display();
	if (ret < 0) {
		LOG_ERR("physical display clear failed: %d", ret);
		oled_raw_ready = false;
		return ret;
	}

	return 0;
}

/*
 * One-shot raw SSD1306 128x32 production baseline.
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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "oled_gfx_portrait.h"
#include "oled_raw_ssd1306_128x32.h"
#include "oled_smoke_screen.h"

LOG_MODULE_REGISTER(oled_raw_ssd1306_128x32, CONFIG_DISPLAY_LOG_LEVEL);

#define PHYSICAL_WIDTH 128U
#define PHYSICAL_HEIGHT 32U
#define BUFFER_SIZE 512U

BUILD_ASSERT(BUFFER_SIZE == (PHYSICAL_WIDTH * PHYSICAL_HEIGHT / 8U),
	     "SSD1306 128x32 framebuffer must be exactly 512 bytes");

static const char oled_raw_build_marker[] =
	"RUTTIGER_RAW_SSD1306_128X32_PRODUCTION_BASELINE_V1";
static const struct device *const oled_raw_display =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static uint8_t oled_raw_framebuffer[BUFFER_SIZE];
static struct display_capabilities oled_raw_caps;
static bool oled_raw_ready;

static const struct display_buffer_descriptor oled_raw_buffer_descriptor = {
	.buf_size = sizeof(oled_raw_framebuffer),
	.width = PHYSICAL_WIDTH,
	.height = PHYSICAL_HEIGHT,
	.pitch = PHYSICAL_WIDTH,
};

uint8_t *oled_raw_ssd1306_128x32_framebuffer(void)
{
	return oled_raw_framebuffer;
}

const struct display_capabilities *
oled_raw_ssd1306_128x32_capabilities(void)
{
	return oled_raw_ready ? &oled_raw_caps : NULL;
}

int oled_raw_ssd1306_128x32_flush(void)
{
	int ret;

	if (!oled_raw_ready) {
		return -ENODEV;
	}

	ret = display_write(oled_raw_display, 0, 0,
			    &oled_raw_buffer_descriptor,
			    oled_raw_framebuffer);
	if (ret < 0) {
		return ret;
	}

	return display_blanking_off(oled_raw_display);
}

static bool oled_raw_capabilities_supported(void)
{
	return oled_raw_caps.x_resolution == PHYSICAL_WIDTH &&
	       oled_raw_caps.y_resolution == PHYSICAL_HEIGHT &&
	       (oled_raw_caps.supported_pixel_formats & PIXEL_FORMAT_MONO10) &&
	       oled_raw_caps.current_pixel_format == PIXEL_FORMAT_MONO10 &&
	       (oled_raw_caps.screen_info & SCREEN_INFO_MONO_VTILED);
}

/* Zephyr v0.3 has no public display_clear() operation. Reuse the one buffer. */
static int oled_raw_clear_display(void)
{
	memset(oled_raw_framebuffer, 0, sizeof(oled_raw_framebuffer));

	return display_write(oled_raw_display, 0, 0,
			     &oled_raw_buffer_descriptor,
			     oled_raw_framebuffer);
}

static void oled_raw_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);

	LOG_INF("build marker: %s", oled_raw_build_marker);

	if (!device_is_ready(oled_raw_display)) {
		LOG_ERR("display device %s is not ready; baseline skipped",
			oled_raw_display->name);
		return;
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
		return;
	}

	oled_raw_ready = true;
	ret = oled_raw_clear_display();
	if (ret < 0) {
		LOG_ERR("physical display clear failed: %d", ret);
		oled_raw_ready = false;
		return;
	}

	oled_gfx_init();
	oled_smoke_screen_draw();
	ret = oled_gfx_flush();
	if (ret < 0) {
		LOG_ERR("128x32 smoke-screen flush failed: %d", ret);
		return;
	}

	LOG_INF("raw SSD1306 128x32 production baseline written once; "
		"framebuffer=%u bytes", BUFFER_SIZE);
}

static K_WORK_DELAYABLE_DEFINE(oled_raw_work, oled_raw_work_handler);

static int oled_raw_schedule(void)
{
	int ret = k_work_schedule(&oled_raw_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule deferred raw OLED baseline: %d", ret);
	}

	/* OLED errors must never prevent the keyboard from booting. */
	return 0;
}

SYS_INIT(oled_raw_schedule, APPLICATION, 0);
